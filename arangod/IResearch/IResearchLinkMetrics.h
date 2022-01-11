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

#include "Metrics/BatchBuilder.h"
#include "Metrics/GaugeBuilder.h"

namespace arangodb::iresearch {

DECLARE_GAUGE(arangosearch_num_buffered_docs, uint64_t,
              "Number of buffered documents");
DECLARE_GAUGE(arangosearch_num_docs, uint64_t, "Number of documents");
DECLARE_GAUGE(arangosearch_num_live_docs, uint64_t, "Number of live documents");
DECLARE_GAUGE(arangosearch_num_segments, uint64_t, "Number of segments");
DECLARE_GAUGE(arangosearch_num_files, uint64_t, "Number of files");
DECLARE_GAUGE(arangosearch_index_size, uint64_t, "Size of the index in bytes");
DECLARE_GAUGE(arangosearch_num_failed_commits, uint64_t,
              "Number of failed commits");
DECLARE_GAUGE(arangosearch_num_failed_cleanups, uint64_t,
              "Number of failed cleanups");
DECLARE_GAUGE(arangosearch_num_failed_consolidations, uint64_t,
              "Number of failed consolidations");
DECLARE_GAUGE(arangosearch_commit_time, uint64_t,
              "Average time of few last commits");
DECLARE_GAUGE(arangosearch_cleanup_time, uint64_t,
              "Average time of few last cleanups");
DECLARE_GAUGE(arangosearch_consolidation_time, uint64_t,
              "Average time of few last consolidations");

}  // namespace arangodb::iresearch
