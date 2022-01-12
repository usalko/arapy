////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "IResearchLinkCoordinator.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <memory>

#include "Basics/StringUtils.h"
#include "ClusterEngine/ClusterEngine.h"
#include "IResearchCommon.h"
#include "IResearchFeature.h"
#include "IResearchLinkHelper.h"
#include "Indexes/IndexFactory.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBIndex.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "Metrics/ClusterMetricsFeature.h"
#include "IResearch/IResearchLinkMetrics.h"

namespace {

using namespace arangodb;

ClusterEngineType getEngineType(
    application_features::ApplicationServer& server) {
#ifdef ARANGODB_USE_GOOGLE_TESTS
  // during the unit tests there is a mock storage engine which cannot be casted
  // to a ClusterEngine at all. the only sensible way to find out the engine
  // type is to try a dynamic_cast here and assume the MockEngine if the cast
  // goes wrong
  auto& engine = server.getFeature<EngineSelectorFeature>().engine();
  auto cast = dynamic_cast<ClusterEngine*>(&engine);
  if (cast != nullptr) {
    return cast->engineType();
  }
  return ClusterEngineType::MockEngine;
#else
  return server.getFeature<arangodb::EngineSelectorFeature>()
      .engine<arangodb::ClusterEngine>()
      .engineType();
#endif
}

}  // namespace

namespace arangodb::iresearch {

IResearchLinkCoordinator::IResearchLinkCoordinator(
    IndexId id, LogicalCollection& collection)
    : arangodb::ClusterIndex(
          id, collection, ::getEngineType(collection.vocbase().server()),
          arangodb::Index::TRI_IDX_TYPE_IRESEARCH_LINK,
          IResearchLinkHelper::emptyIndexSlice(0)
              .slice()),  // we don`t have objectId`s on coordinator
      IResearchLink(id, collection) {
  TRI_ASSERT(ServerState::instance()->isCoordinator());
  _unique = false;  // cannot be unique since multiple fields are indexed
  _sparse = true;   // always sparse
}

Result IResearchLinkCoordinator::init(
    velocypack::Slice definition,
    IResearchLink::InitCallback const& initCallback) {
  auto r = IResearchLink::init(definition, initCallback);
  auto& metric = IResearchLink::collection()
                     .vocbase()
                     .server()
                     .getFeature<metrics::ClusterMetricsFeature>();
  metric.asyncUpdate();
  std::function batchToCoordinator =
      [](metrics::ClusterMetricsFeature::CoordinatorMetrics& metrics,
         std::string_view name, velocypack::Slice labels,
         velocypack::Slice value) {
        auto& cell = std::get<uint64_t>(
            metrics[metrics::ClusterMetricsFeature::MetricKey{
                name, labels.stringView()}]);
        cell += value.getUIntUnchecked();
      };
  metrics::ClusterMetricsFeature::ToPrometheus batchToPrometheus{
      [](std::string& result, std::string_view name) {
        result.append("# HELP ").append(name);
        result.push_back(' ');
        result.append("NO HELP");  // TODO(MBkkt) Should be help string
        result.push_back('\n');
        result.append("# TYPE ").append(name).append(" gauge\n");
      },
      [](std::string& result, std::string_view globals,
         metrics::ClusterMetricsFeature::MetricKey const& key,
         metrics::ClusterMetricsFeature::MetricValue const& value) {
        result.append(key.first);
        result.push_back('{');
        result.append(globals);
        if (!key.second.empty()) {
          if (!globals.empty()) {
            result.push_back(',');
          }
          result.append(key.second);
        }
        result.push_back('}');
        result.append(std::to_string(std::get<uint64_t>(value)));
        result.push_back('\n');
      }};
  metric.add(arangosearch_num_docs::kName, batchToCoordinator,
             batchToPrometheus);
  metric.add(arangosearch_num_live_docs::kName, batchToCoordinator,
             batchToPrometheus);
  metric.add(arangosearch_num_segments::kName, batchToCoordinator,
             batchToPrometheus);
  metric.add(arangosearch_num_files::kName, batchToCoordinator,
             batchToPrometheus);
  metric.add(arangosearch_index_size::kName, batchToCoordinator,
             batchToPrometheus);
  std::function gaugeToCoordinator =
      [](metrics::ClusterMetricsFeature::CoordinatorMetrics& metrics,
         std::string_view name, velocypack::Slice labels,
         velocypack::Slice value) {
        auto labelsStr = labels.stringView();
        auto& cell = std::get<uint64_t>(
            metrics[metrics::ClusterMetricsFeature::MetricKey{
                name, labelsStr.substr(0, labelsStr.find(",shard=\""))}]);
        cell += value.getUIntUnchecked();
      };
  metric.add(arangosearch_num_failed_commits::kName, gaugeToCoordinator);
  metric.add(arangosearch_num_failed_cleanups::kName, gaugeToCoordinator);
  metric.add(arangosearch_num_failed_consolidations::kName, gaugeToCoordinator);
  metric.add(arangosearch_commit_time::kName, gaugeToCoordinator);
  metric.add(arangosearch_cleanup_time::kName, gaugeToCoordinator);
  metric.add(arangosearch_consolidation_time::kName, gaugeToCoordinator);
  return r;
}

void IResearchLinkCoordinator::toVelocyPack(
    velocypack::Builder& builder,
    std::underlying_type<Index::Serialize>::type flags) const {
  if (builder.isOpenObject()) {
    THROW_ARANGO_EXCEPTION(Result(
        TRI_ERROR_BAD_PARAMETER,
        std::string("failed to generate link definition for arangosearch view "
                    "Cluster link '") +
            std::to_string(Index::id().id()) + "'"));
  }

  auto forPersistence = Index::hasFlag(flags, Index::Serialize::Internals);

  builder.openObject();

  if (!properties(builder, forPersistence).ok()) {
    THROW_ARANGO_EXCEPTION(Result(
        TRI_ERROR_INTERNAL,
        std::string("failed to generate link definition for arangosearch view "
                    "Cluster link '") +
            std::to_string(Index::id().id()) + "'"));
  }

  if (Index::hasFlag(flags, Index::Serialize::Figures)) {
    builder.add("figures", VPackValue(VPackValueType::Object));
    toVelocyPackFigures(builder);
    builder.close();
  }

  builder.close();
}

IResearchLinkCoordinator::IndexFactory::IndexFactory(
    application_features::ApplicationServer& server)
    : IndexTypeFactory(server) {}

bool IResearchLinkCoordinator::IndexFactory::equal(
    velocypack::Slice lhs, velocypack::Slice rhs,
    std::string const& dbname) const {
  return IResearchLinkHelper::equal(_server, lhs, rhs, dbname);
}

std::shared_ptr<Index> IResearchLinkCoordinator::IndexFactory::instantiate(
    LogicalCollection& collection, VPackSlice definition, IndexId id,
    bool /*isClusterConstructor*/) const {
  auto link = std::make_shared<IResearchLinkCoordinator>(id, collection);
  auto res = link->init(definition);

  if (!res.ok()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  return link;
}

Result IResearchLinkCoordinator::IndexFactory::normalize(
    velocypack::Builder& normalized, velocypack::Slice definition,
    bool isCreation, TRI_vocbase_t const& vocbase) const {
  // no attribute set in a definition -> old version
  constexpr LinkVersion defaultVersion = LinkVersion::MIN;
  return IResearchLinkHelper::normalize(normalized, definition, isCreation,
                                        vocbase, defaultVersion);
}

std::shared_ptr<IResearchLinkCoordinator::IndexFactory>
IResearchLinkCoordinator::createFactory(
    application_features::ApplicationServer& server) {
  return std::make_shared<IResearchLinkCoordinator::IndexFactory>(server);
}

}  // namespace arangodb::iresearch
