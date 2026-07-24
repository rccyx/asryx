#include "config/config.hpp"
#include "constants/constants.hpp"
#include "model/model.hpp"
#include "tests/model_store.hpp"
#include "tests/tests.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <libassert/assert.hpp>

void run_test_model()
{
  const auto& supported = model::get_supported_models();
  ASSERT(!supported.empty());
  ASSERT(std::find(supported.begin(), supported.end(),
                   std::string(constants::config::default_model)) != supported.end());

  const auto& languages = model::get_supported_languages();
  ASSERT(std::find(languages.begin(), languages.end(), std::string("fr")) != languages.end());
  ASSERT(std::find(languages.begin(), languages.end(), std::string("ar")) != languages.end());
  ASSERT(std::find(languages.begin(), languages.end(), std::string("yue")) != languages.end());
  ASSERT(model::is_supported_language(std::string(constants::config::auto_language)));
  ASSERT(!model::is_supported_language("jrnfejfef"));

  const auto default_model_path =
      model::get_model_path(std::string(constants::config::default_model));
  ASSERT(default_model_path.has_value());
  const std::string& path = *default_model_path;
  ASSERT(!path.empty());

  const auto vad_model_path = model::get_vad_model_path();
  ASSERT(vad_model_path.has_value());
  ASSERT(!vad_model_path->empty());

  const auto vad_installed = model::is_vad_model_installed();
  ASSERT(vad_installed.has_value());
  ASSERT(!*vad_installed);

  const auto missing_vad = model::validate_vad_model();
  ASSERT(!missing_vad.has_value());
  ASSERT(missing_vad.error().message == "VAD model is not installed: " + *vad_model_path);

  const auto default_model_installed =
      model::is_model_installed(std::string(constants::config::default_model));
  ASSERT(default_model_installed.has_value());
  ASSERT(!*default_model_installed);

  const auto missing_model = model::use_model(std::string(constants::config::default_model));
  ASSERT(!missing_model.has_value());
  ASSERT(missing_model.error().message ==
         "model 'base.en' is not installed. Install it with: asryx --model install base.en");

  model_store::write_model(std::string(constants::config::default_model));
  ASSERT(model::use_model(std::string(constants::config::default_model)).has_value());
  ASSERT(model::use_language(std::string(constants::config::english_language)).has_value());

  auto loaded_config = config::load_config();
  ASSERT(loaded_config.has_value());
  auto cfg = *loaded_config;
  ASSERT(cfg.model == std::string(constants::config::default_model));
  ASSERT(cfg.language == std::string(constants::config::english_language));

  auto language = model::transcription_language_for(cfg);
  ASSERT(language.has_value());
  ASSERT(*language == std::string(constants::config::english_language));

  const auto invalid_language = model::use_language("jrnfejfef");
  ASSERT(!invalid_language.has_value());
  ASSERT(invalid_language.error().message == "unsupported language: jrnfejfef");

  loaded_config = config::load_config();
  ASSERT(loaded_config.has_value());
  cfg = *loaded_config;
  ASSERT(cfg.language == std::string(constants::config::english_language));

  const auto english_only_language = model::use_language("fr");
  ASSERT(!english_only_language.has_value());
  ASSERT(english_only_language.error().message ==
         "active model base.en is English-only; use a multilingual model for fr");

  loaded_config = config::load_config();
  ASSERT(loaded_config.has_value());
  cfg = *loaded_config;
  ASSERT(cfg.language == std::string(constants::config::english_language));

  model_store::write_model("base");
  ASSERT(model::use_model("base").has_value());
  ASSERT(model::use_language("fr").has_value());

  loaded_config = config::load_config();
  ASSERT(loaded_config.has_value());
  cfg = *loaded_config;
  ASSERT(cfg.model == std::string("base"));
  ASSERT(cfg.language == std::string("fr"));

  language = model::transcription_language_for(cfg);
  ASSERT(language.has_value());
  ASSERT(*language == std::string("fr"));

  const auto english_only_model = model::use_model(std::string(constants::config::default_model));
  ASSERT(!english_only_model.has_value());
  ASSERT(english_only_model.error().message ==
         "active model base.en is English-only; use a multilingual model for fr");

  loaded_config = config::load_config();
  ASSERT(loaded_config.has_value());
  cfg = *loaded_config;
  ASSERT(cfg.model == std::string("base"));
  ASSERT(cfg.language == std::string("fr"));

  ASSERT(model::use_language(std::string(constants::config::auto_language)).has_value());

  loaded_config = config::load_config();
  ASSERT(loaded_config.has_value());
  cfg = *loaded_config;
  language = model::transcription_language_for(cfg);
  ASSERT(language.has_value());
  ASSERT(*language == std::string(""));

  ASSERT(model::use_model(std::string(constants::config::default_model)).has_value());

  const auto active_uninstall =
      model::uninstall_model(std::string(constants::config::default_model));
  ASSERT(!active_uninstall.has_value());
  ASSERT(active_uninstall.error().message ==
         "cannot uninstall active model 'base.en'; switch models first with: asryx --model use "
         "<other>");
  ASSERT(std::filesystem::exists(path));

  std::cout << "test_model passed\n";
}
