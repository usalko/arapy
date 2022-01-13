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

#include "Metrics/ClusterMetricsFeature.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterMethods.h"
#include "Metrics/MetricsFeature.h"
#include "Network/NetworkFeature.h"
#include "Scheduler/SchedulerFeature.h"

namespace arangodb::metrics {
namespace {

using namespace std::chrono_literals;
constexpr std::chrono::steady_clock::duration kTimeout = 15s;

}  // namespace

ClusterMetricsFeature::ClusterMetricsFeature(
    application_features::ApplicationServer& server)
    : ApplicationFeature{server, "ClusterMetrics"} {
  setOptional();
  startsAfter<ClusterFeature>();
  startsAfter<NetworkFeature>();
  startsAfter<SchedulerFeature>();
}

void ClusterMetricsFeature::validateOptions(
    std::shared_ptr<options::ProgramOptions> /*options*/) {
  if (!ServerState::instance()->isCoordinator()) {
    disable();
  }
}

void ClusterMetricsFeature::asyncUpdate() {
  if (isEnabled() && _count.fetch_add(1, std::memory_order_acq_rel) == 0) {
    forceAsyncUpdate();
  }
}

void ClusterMetricsFeature::forceAsyncUpdate() {
  auto internal = [this] {
    _count.store(1, std::memory_order_relaxed);
    // TODO try get from agency
    metricsOnCoordinator(server().getFeature<NetworkFeature>(),
                         server().getFeature<ClusterFeature>())
        .then([this](futures::Try<RawDBServers>&& metrics) mutable {
          bool needRetry = !metrics.hasValue();
          if (!needRetry) {
            _lastUpdate = std::chrono::steady_clock::now();
            // We want more time than kTimeout should expire after last cross
            // cluster communication
            set(std::move(*metrics));
            needRetry = _count.fetch_sub(1, std::memory_order_acq_rel) != 1;
            // if true then was call asyncUpdate() after _count.store(1)
          }
          if (needRetry) {
            forceAsyncUpdate();
          }
        });  // detach() here
  };
  auto now = std::chrono::steady_clock::now();
  if (now < _lastUpdate + kTimeout) {
    _handle = SchedulerFeature::SCHEDULER->queueDelayed(
        RequestLane::CLUSTER_INTERNAL, _lastUpdate + kTimeout - now,
        [&](bool canceled) {
          if (!canceled) {
            internal();
          }
        });
  } else {
    _handle = nullptr;
    SchedulerFeature::SCHEDULER->queue(RequestLane::CLUSTER_INTERNAL, internal);
  }
}

void ClusterMetricsFeature::set(RawDBServers&& raw) const {
  CoordinatorMetrics metrics;
  {
    std::shared_lock lock{_m};
    for (auto const& payload : raw) {
      TRI_ASSERT(payload);
      velocypack::Slice slice{payload->data()};
      size_t size = slice.length();
      TRI_ASSERT(size % 3 == 0);
      for (size_t i = 0; i != size; i += 3) {
        auto name = slice.at(i).stringView();
        auto labels = slice.at(i + 1);
        auto value = slice.at(i + 2);
        auto f = _toCoordinator.find(name);
        if (f != _toCoordinator.end()) {
          f->second(metrics, name, labels, value);
        }
      }
    }
  }
  auto data = std::make_shared<Data>(std::move(metrics));
  std::atomic_store_explicit(&_data, data, std::memory_order_release);
  // TODO(MBkkt) serialize to velocypack array
  // TODO(MBkkt) set to agency
}

void ClusterMetricsFeature::add(std::string_view metric,
                                ToCoordinator const& toCoordinator) {
  std::lock_guard lock{_m};
  _toCoordinator[metric] = toCoordinator;
}

void ClusterMetricsFeature::add(std::string_view metric,
                                ToCoordinator const& toCoordinator,
                                ToPrometheus const& toPrometheus) {
  std::lock_guard lock{_m};
  _toCoordinator[metric] = toCoordinator;
  _toPrometheus[metric] = toPrometheus;
}

void ClusterMetricsFeature::toPrometheus(std::string& result,
                                         std::string_view globals) const {
  auto data = std::atomic_load_explicit(&_data, std::memory_order_acquire);
  if (!data) {
    return;
  }
  std::shared_lock lock{_m};
  std::string_view metricName;
  auto it = _toPrometheus.end();
  for (auto const& [key, value] : data->metrics) {
    if (metricName != key.first) {
      metricName = key.first;
      it = _toPrometheus.find(metricName);
      if (it != _toPrometheus.end()) {
        it->second.begin(result, metricName);
      }
    }
    if (it != _toPrometheus.end()) {
      it->second.each(result, globals, key, value);
    }
  }
}

}  // namespace arangodb::metrics
