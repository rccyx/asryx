#include "config/config.hpp"
#include "constants/constants.hpp"
#include "error.hpp"
#include "model/model.hpp"
#include "model/store.hpp"
#include "platform/fs.hpp"
#include "tests/model_store.hpp"
#include "tests/tests.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <libassert/assert.hpp>
#include <sstream>
#include <string>

namespace {

std::filesystem::path _model_path(const std::string& name)
{
  const auto path = model::get_model_path(name);
  ASSERT(path.has_value());
  return {*path};
}

std::filesystem::path _model_tmp_path(const std::string& name)
{
  const auto path = _model_path(name);
  return path.parent_path() / ("." + path.filename().string() + ".tmp");
}

std::filesystem::path _quantizer_marker_path()
{
  const auto path = model::store::model_dir();
  ASSERT(path.has_value());
  return *path / ".quantizer.marker";
}

void _delete_if_exists(const std::filesystem::path& path)
{
  yx::ignore_failure(platform::safe_delete_file(path));
}

void _delete_quantizer()
{
  const auto path = model::store::whisper_model_quantizer();
  ASSERT(path.has_value());
  _delete_if_exists(*path);
}

void _write_quantizer(bool succeeds)
{
  const auto path = model::store::whisper_model_quantizer();
  ASSERT(path.has_value());

  std::filesystem::create_directories(path->parent_path());
  std::ofstream file(*path);
  file << "#!/bin/sh\n"
       << R"(printf '%s %s %s\n' "$1" "$2" "$3" > ')" << _quantizer_marker_path().string() << "'\n";

  if (succeeds) {
    file << "cp \"$1\" \"$2\"\nexit 0\n";
  }
  else {
    file << "exit 17\n";
  }

  file.close();
  std::filesystem::permissions(*path, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
}

std::string _captured_model_list()
{
  std::ostringstream output;
  auto* const old_buffer = std::cout.rdbuf(output.rdbuf());
  const auto listed = model::list_models();
  std::cout.rdbuf(old_buffer);
  ASSERT(listed.has_value());
  return output.str();
}

void _assert_contains(const std::string& text, const std::string& needle)
{
  ASSERT(text.find(needle) != std::string::npos);
}

void _test_model_catalog()
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

  const auto& quant_types = model::get_supported_quant_types();
  ASSERT(std::find(quant_types.begin(), quant_types.end(), std::string("q5_0")) !=
         quant_types.end());
  ASSERT(std::find(quant_types.begin(), quant_types.end(), std::string("q6_k")) !=
         quant_types.end());
  ASSERT(std::find(quant_types.begin(), quant_types.end(), std::string("8")) == quant_types.end());
}

std::string _test_missing_default_model()
{
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

  return *default_model_path;
}

void _test_language_selection()
{
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
}

void _test_model_quantization()
{
  _delete_quantizer();
  const auto missing_quantizer = model::quantize_model("base", "q5_0");
  ASSERT(!missing_quantizer.has_value());
  ASSERT(
      missing_quantizer.error().message.starts_with("failed to quantize base as q5_0: "
                                                    "missing whisper.cpp quantizer: "));
  ASSERT(!std::filesystem::exists(_model_path("base-q5_0")));

  const auto invalid_quant = model::quantize_model("base", "banana");
  ASSERT(!invalid_quant.has_value());
  ASSERT(invalid_quant.error().message ==
         "unsupported quant type: banana\nsupported quant types: q4_0, q4_1, q5_0, q5_1, "
         "q8_0, q2_k, q3_k, q4_k, q5_k, q6_k");

  const auto numeric_quant = model::quantize_model("base", "8");
  ASSERT(!numeric_quant.has_value());
  ASSERT(numeric_quant.error().message.starts_with("unsupported quant type: 8"));

  const auto missing_source = model::quantize_model("small", "q5_0");
  ASSERT(!missing_source.has_value());
  ASSERT(missing_source.error().message ==
         "model 'small' is not installed. Install it with: asryx --model install small");

  _write_quantizer(false);
  const auto failed_quantizer = model::quantize_model("base", "q5_0");
  ASSERT(!failed_quantizer.has_value());
  ASSERT(failed_quantizer.error().message ==
         "failed to quantize base as q5_0: failed to quantize model");
  ASSERT(!std::filesystem::exists(_model_path("base-q5_0")));
  ASSERT(!std::filesystem::exists(_model_tmp_path("base-q5_0")));

  _delete_if_exists(_quantizer_marker_path());
  _write_quantizer(true);
  ASSERT(model::quantize_model("base", "q5_0").has_value());
  ASSERT(std::filesystem::exists(_model_path("base-q5_0")));
  ASSERT(!std::filesystem::exists(_model_tmp_path("base-q5_0")));
  ASSERT(std::filesystem::exists(_quantizer_marker_path()));

  _delete_if_exists(_quantizer_marker_path());
  ASSERT(model::quantize_model("base", "q5_0").has_value());
  ASSERT(!std::filesystem::exists(_quantizer_marker_path()));

  const std::string listed_models = _captured_model_list();
  _assert_contains(listed_models, "base-q5_0 (installed)");

  ASSERT(model::use_model("base-q5_0").has_value());
  ASSERT(model::use_language("fr").has_value());

  model_store::write_model(std::string(constants::config::default_model));
  ASSERT(model::quantize_model(std::string(constants::config::default_model), "q5_1").has_value());
  const auto english_quantized_model =
      model::use_model(std::string(constants::config::default_model) + "-q5_1");
  ASSERT(!english_quantized_model.has_value());
  ASSERT(english_quantized_model.error().message ==
         "active model base.en-q5_1 is English-only; use a multilingual model for fr");

  auto loaded_config = config::load_config();
  ASSERT(loaded_config.has_value());
  const auto& cfg = *loaded_config;
  auto language = model::transcription_language_for(cfg);
  ASSERT(language.has_value());
  ASSERT(*language == std::string("fr"));

  ASSERT(model::use_language(std::string(constants::config::auto_language)).has_value());
  ASSERT(model::use_model(std::string(constants::config::default_model)).has_value());
  ASSERT(model::quantize_model("base", "q5_1").has_value());
  ASSERT(model::use_model(std::string(constants::config::default_model)).has_value());

  ASSERT(model::uninstall_model("base-q5_0").has_value());
  ASSERT(!std::filesystem::exists(_model_path("base-q5_0")));
  ASSERT(std::filesystem::exists(_model_path("base")));

  ASSERT(model::uninstall_model("base").has_value());
  ASSERT(!std::filesystem::exists(_model_path("base")));
  ASSERT(std::filesystem::exists(_model_path("base-q5_1")));
}

void _test_active_model_uninstall(const std::string& path)
{
  const auto active_uninstall =
      model::uninstall_model(std::string(constants::config::default_model));
  ASSERT(!active_uninstall.has_value());
  ASSERT(active_uninstall.error().message ==
         "cannot uninstall active model 'base.en'; switch models first with: asryx --model use "
         "<other>");
  ASSERT(std::filesystem::exists(path));
}

} // namespace

void run_test_model()
{
  _test_model_catalog();
  const std::string path = _test_missing_default_model();
  _test_language_selection();
  _test_model_quantization();
  _test_active_model_uninstall(path);

  std::cout << "test_model passed\n";
}
