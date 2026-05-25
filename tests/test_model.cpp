#include "config/config.hpp"
#include "constants/constants.hpp"
#include "model/model.hpp"
#include "tests/test_helpers.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {

void write_fake_model(const std::string& name)
{
  const auto path = std::filesystem::path(model::get_model_path(name));
  std::filesystem::create_directories(path.parent_path());
  std::ofstream model_file(path);
  model_file << "fake model content";
}

template <typename Fn> bool runtime_error_equals(Fn fn, const std::string& expected)
{
  try {
    fn();
  }
  catch (const std::runtime_error& e) {
    return std::string(e.what()) == expected;
  }

  return false;
}

} // namespace

void run_test_model()
{
  const auto& supported = model::get_supported_models();
  ASSERT_TRUE(!supported.empty());
  ASSERT_TRUE(std::find(supported.begin(), supported.end(),
                        std::string(constants::config::default_model)) != supported.end());

  const auto& languages = model::get_supported_languages();
  ASSERT_TRUE(std::find(languages.begin(), languages.end(), std::string("fr")) != languages.end());
  ASSERT_TRUE(std::find(languages.begin(), languages.end(), std::string("ar")) != languages.end());
  ASSERT_TRUE(std::find(languages.begin(), languages.end(), std::string("yue")) != languages.end());
  ASSERT_TRUE(model::is_supported_language(std::string(constants::config::auto_language)));
  ASSERT_FALSE(model::is_supported_language("jrnfejfef"));

  std::string path = model::get_model_path(std::string(constants::config::default_model));
  ASSERT_TRUE(!path.empty());

  ASSERT_FALSE(model::is_model_installed(std::string(constants::config::default_model)));
  ASSERT_TRUE(runtime_error_equals(
      [] { model::use_model(std::string(constants::config::default_model)); },
      "model 'base.en' is not installed. Install it with: asryx --model install base.en"));

  write_fake_model(std::string(constants::config::default_model));
  model::use_model(std::string(constants::config::default_model));
  model::use_language(std::string(constants::config::english_language));

  auto cfg = config::load_config();
  ASSERT_EQ(cfg.model, std::string(constants::config::default_model));
  ASSERT_EQ(cfg.language, std::string(constants::config::english_language));
  ASSERT_EQ(model::transcription_language_for(cfg),
            std::string(constants::config::english_language));

  ASSERT_TRUE(runtime_error_equals([] { model::use_language("jrnfejfef"); },
                                   "unsupported language: jrnfejfef"));
  cfg = config::load_config();
  ASSERT_EQ(cfg.language, std::string(constants::config::english_language));

  ASSERT_TRUE(runtime_error_equals(
      [] { model::use_language("fr"); },
      "active model base.en is English-only; use a multilingual model for fr"));
  cfg = config::load_config();
  ASSERT_EQ(cfg.language, std::string(constants::config::english_language));

  write_fake_model("base");
  model::use_model("base");
  model::use_language("fr");
  cfg = config::load_config();
  ASSERT_EQ(cfg.model, std::string("base"));
  ASSERT_EQ(cfg.language, std::string("fr"));
  ASSERT_EQ(model::transcription_language_for(cfg), std::string("fr"));

  ASSERT_TRUE(runtime_error_equals(
      [] { model::use_model(std::string(constants::config::default_model)); },
      "active model base.en is English-only; use a multilingual model for fr"));
  cfg = config::load_config();
  ASSERT_EQ(cfg.model, std::string("base"));
  ASSERT_EQ(cfg.language, std::string("fr"));

  model::use_language(std::string(constants::config::auto_language));
  cfg = config::load_config();
  ASSERT_EQ(model::transcription_language_for(cfg), std::string(""));

  model::use_model(std::string(constants::config::default_model));

  bool active_uninstall_rejected = false;
  try {
    model::uninstall_model(std::string(constants::config::default_model));
  }
  catch (const std::runtime_error& e) {
    active_uninstall_rejected =
        std::string(e.what()) ==
        "cannot uninstall active model 'base.en'; switch models first with: asryx --model use "
        "<other>";
  }
  ASSERT_TRUE(active_uninstall_rejected);
  ASSERT_TRUE(std::filesystem::exists(path));

  std::cout << "test_model passed\n";
}
