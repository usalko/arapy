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
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <unicode/locid.h>
#include <memory>
#include <string>

#include "ApplicationFeatures/ApplicationFeature.h"

namespace arangodb {
namespace application_features {
class GreetingsFeaturePhase;
}
namespace options {
class ProgramOptions;
}

class LanguageFeature final : public application_features::ApplicationFeature {
 public:
  static constexpr std::string_view name() noexcept { return "Language"; }

  template<typename Server>
  explicit LanguageFeature(Server& server)
      : application_features::ApplicationFeature{server, *this},
        _locale(),
        _binaryPath(server.getBinaryPath()),
        _icuDataPtr(nullptr),
        _forceLanguageCheck(true) {
    setOptional(false);
    startsAfter<application_features::GreetingsFeaturePhase, Server>();
  }

  ~LanguageFeature();

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;
  void start() override final;
  static void* prepareIcu(std::string const& binaryPath,
                          std::string const& binaryExecutionPath,
                          std::string& path, std::string const& binaryName);
  icu::Locale& getLocale();
  std::string const& getDefaultLanguage() const;
  bool forceLanguageCheck() const;
  std::string getCollatorLanguage() const;
  void resetDefaultLanguage(std::string const& language);

 private:
  icu::Locale _locale;
  std::string _language;
  char const* _binaryPath;
  void* _icuDataPtr;
  bool _forceLanguageCheck;
};

}  // namespace arangodb
