////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
/// @author Copyright 2017-2018, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arangodb::velocypack {
class Slice;
struct Options;
}  // namespace arangodb::velocypack

namespace arangodb::validation {

struct JsonSchema;

struct ValidationError {
  std::vector<std::string> path;
  std::string message;
};

[[nodiscard]] std::optional<ValidationError> validate(
    JsonSchema const& schema, velocypack::Slice doc,
    velocypack::Options const* opts);

[[nodiscard]] std::shared_ptr<JsonSchema const> new_schema(
    velocypack::Slice schema);

}  // namespace arangodb::validation
