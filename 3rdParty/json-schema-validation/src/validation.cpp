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

#include <variant>

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <validation/validation.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/adapters/adapter.hpp>
#include <valijson/adapters/basic_adapter.hpp>

using namespace arangodb;
using namespace arangodb::validation;

struct validation::JsonSchema {
  valijson::Schema mySchema;
};

namespace {
#if 0
struct VelocyPackValue {
  bool isObject() const { return slice.isObject(); }
  bool isArray() const { return slice.isArray(); }
  bool isInteger() const { return slice.isInteger(); }
  bool isDouble() const { return slice.isDouble(); }
  bool isString() const { return slice.isString(); }
  bool isBool() const { return slice.isBool(); }
  bool isNull() const { return slice.isNull(); }

  bool getString(std::string& result) {
    if (isString()) {
      result = slice.copyString();
    }
  }

  velocypack::Slice slice;
};

struct VelocyPackMember {};
struct VelocyPackObject {
  struct EndSentinel {};
  EndSentinel end() const { return EndSentinel{}; }



  struct Iterator {
    std::variant<velocypack::ObjectIterator, velocypack::Slice> iter;

    friend bool operator==(Iterator const& s, EndSentinel const&) noexcept {
      struct Visitor {
        bool operator()(velocypack::Slice s) { return s.isNone(); }
        bool operator()(velocypack::ObjectIterator const& i) {
          return i.valid();
        }
      };

      return std::visit(Visitor{}, s.iter);
    }

    valijson::adapters::DerefProxy<
  };

  using const_iterator = Iterator;

  Iterator begin() const { return Iterator{velocypack::ObjectIterator{slice}}; }
  Iterator find(std::string_view field) const {
    return Iterator{slice.get(field)};
  }
  std::size_t size() const noexcept { return slice.length(); }

  velocypack::Slice slice;
};
struct VelocyPackArray {};

struct VelocyPackAdapter;

using BaseType =
    valijson::adapters::BasicAdapter<VelocyPackAdapter, VelocyPackArray,
                                     VelocyPackMember, VelocyPackObject,
                                     VelocyPackValue>;

struct VelocyPackAdapter : BaseType {
  explicit VelocyPackAdapter(velocypack::Slice slice)
      : BaseType(VelocyPackValue{slice}) {}
  explicit VelocyPackAdapter(VelocyPackValue v) : BaseType(v) {}
};
#endif

struct VelocyPackAdapterNew : valijson::adapters::Adapter {
  explicit VelocyPackAdapterNew(velocypack::Slice slice) : slice(slice) {}

  bool applyToArray(ArrayValueCallback fn) const override { return false; }
  bool applyToObject(ObjectMemberCallback fn) const override { return false; }
  bool asBool() const override { return false; }
  bool asBool(bool& result) const override { return false; }
  double asDouble() const override { return 0; }
  bool asDouble(double& result) const override { return false; }
  int64_t asInteger() const override { return 0; }
  bool asInteger(int64_t& result) const override { return false; }
  std::string asString() const override { return std::string(); }
  bool asString(std::string& result) const override { return false; }
  bool equalTo(Adapter const& other, bool strict) const override {
    return false;
  }
  valijson::adapters::FrozenValue* freeze() const override { return nullptr; }
  size_t getArraySize() const override { return 0; }
  bool getArraySize(size_t& result) const override { return false; }
  bool getBool() const override { return false; }
  bool getBool(bool& result) const override { return false; }
  double getDouble() const override { return 0; }
  bool getDouble(double& result) const override { return false; }
  int64_t getInteger() const override { return 0; }
  bool getInteger(int64_t& result) const override { return false; }
  double getNumber() const override { return 0; }
  bool getNumber(double& result) const override { return false; }
  size_t getObjectSize() const override {
    std::size_t result;
    if (getObjectSize(result)) {
      return result;
    }

    valijson::throwRuntimeError("JSON value is not an object.");
  }
  bool getObjectSize(size_t& result) const override {
    if (isObject()) {
      result = slice.length();
      return true;
    }
    return false;
  }
  std::string getString() const override {
    std::string result;
    if (getString(result)) {
      return result;
    }

    valijson::throwRuntimeError("JSON value is not a string.");
  }
  bool getString(std::string& result) const override {
    if (isString()) {
      result = slice.copyString();
      return true;
    }
    return false;
  }
  bool hasStrictTypes() const override { return true; }
  bool isArray() const override { return slice.isArray(); }
  bool isBool() const override { return slice.isBool(); }
  bool isDouble() const override { return slice.isDouble(); }
  bool isInteger() const override { return slice.isInteger(); }
  bool isNull() const override { return slice.isNull(); }
  bool isNumber() const override { return slice.isNumber(); }
  bool isObject() const override { return slice.isObject(); }
  bool isString() const override { return slice.isString(); }
  bool maybeArray() const override { return isArray(); }
  bool maybeBool() const override { return isBool(); }
  bool maybeDouble() const override { return isDouble(); }
  bool maybeInteger() const override { return isInteger(); }
  bool maybeNull() const override { return isNull(); }
  bool maybeObject() const override { return isObject(); }
  bool maybeString() const override { return isString(); }

 private:
  velocypack::Slice slice;
};

}  // namespace

template<>
struct valijson::adapters::AdapterTraits<VelocyPackAdapterNew> {
  using DocumentType = velocypack::Slice;
};

[[nodiscard]] std::shared_ptr<JsonSchema const> new_schema(
    velocypack::Slice schema) {
  valijson::SchemaParser parser;
  auto ptr = std::make_shared<JsonSchema>();

  VelocyPackAdapterNew adapter(schema);
  parser.populateSchema(adapter, ptr->mySchema);
  return ptr;
}
