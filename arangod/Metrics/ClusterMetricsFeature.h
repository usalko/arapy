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
/// @author Valery Mironov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "Metrics/Parse.h"
#include "Metrics/MetricKey.h"
#include "Scheduler/SchedulerFeature.h"
#include "Containers/FlatHashMap.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <shared_mutex>
#include <variant>

namespace arangodb::metrics {

class ClusterMetricsFeature final
    : public application_features::ApplicationFeature {
 public:
  using MetricValue = std::variant<uint64_t>;
  using MetricKey = std::pair<std::string, std::string>;
  using CoordinatorMetrics = std::map<MetricKey, MetricValue>;
  // We want map because promtool stupid format

  struct Data {
    Data() = default;
    explicit Data(CoordinatorMetrics&& m) : metrics{std::move(m)} {}

    CoordinatorMetrics metrics;
  };

  explicit ClusterMetricsFeature(
      application_features::ApplicationServer& server);

  void validateOptions(std::shared_ptr<options::ProgramOptions> options) final;

  void asyncUpdate();

  /// metric add parse metric
  using ToCoordinator = std::function<void(
      CoordinatorMetrics& /*reduced metrics*/, std::string_view /*metric name*/,
      velocypack::Slice /*metric labels*/, velocypack::Slice /*metric value*/)>;
  struct ToPrometheus {
    using Begin = std::function<void(std::string& /*result*/,
                                     std::string_view /*metric name*/)>;
    using Each =
        std::function<void(std::string& /*result*/,
                           std::string_view /*global this coordinator labels*/,
                           MetricKey const&, MetricValue const&)>;
    Begin begin;
    Each each;
  };
  void add(std::string_view metric, ToCoordinator const& toCoordinator);
  void add(std::string_view metric, ToCoordinator const& toCoordinator,
           ToPrometheus const& toPrometheus);

  void toPrometheus(std::string& result, std::string_view globals) const;

 private:
  mutable std::shared_mutex _m;
  containers::FlatHashMap<std::string_view, ToCoordinator> _toCoordinator;
  containers::FlatHashMap<std::string_view, ToPrometheus> _toPrometheus;

  void forceAsyncUpdate();

  void set(RawDBServers&& metrics) const;

  mutable std::shared_ptr<Data> _data;
  std::atomic_size_t _count{0};
  std::chrono::time_point<std::chrono::steady_clock> _lastUpdate{};
  Scheduler::WorkHandle _handle;
};

}  // namespace arangodb::metrics
