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

#include "Containers/NodeHashMap.h"
#include "Metrics/IBatch.h"

#include <vector>

namespace arangodb::metrics {

template<typename T>
class Batch final : public IBatch {
 public:
  void toPrometheus(std::string& result, std::string_view globals) const final {
    std::vector<typename T::Data> metrics;
    metrics.reserve(_metrics.size());
    for (auto& [_, metric] : _metrics) {
      metrics.push_back(metric.load());  // synchronization here
    }
    for (size_t i = 0; i != T::size(); ++i) {
      for (size_t j = 0; auto& [labels, _] : _metrics) {
        T::toPrometheus(metrics[j], j == 0, i, result, globals, labels);
        ++j;
      }
    }
  }

  T& add(std::string labels) {  //
    return _metrics[std::move(labels)];
  }
  size_t remove(std::string_view labels) final {
    auto const size = _metrics.size();
    return size - _metrics.erase(labels);
  }

 private:
  containers::NodeHashMap<std::string, T> _metrics;
};

}  // namespace arangodb::metrics
