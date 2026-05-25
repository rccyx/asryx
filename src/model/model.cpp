#include "model/model.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace model {

namespace {

bool is_supported_model_name(const std::string& name)
{
  const auto& supported = get_supported_models();
  return std::find(supported.begin(), supported.end(), name) != supported.end();
}

std::filesystem::path model_dir()
{
  return platform::get_home_relative_path(std::string(constants::paths::data_dir_rel));
}

std::filesystem::path whisper_source_dir()
{
  return platform::get_home_relative_path(std::string(constants::paths::whisper_checkout_rel));
}

std::filesystem::path whisper_model_path(const std::string& name)
{
  return whisper_source_dir() / "models" / ("ggml-" + name + ".bin");
}

std::filesystem::path whisper_model_downloader()
{
  return whisper_source_dir() / "models/download-ggml-model.sh";
}

bool file_exists_nonempty(const std::filesystem::path& path)
{
  return std::filesystem::exists(path) && std::filesystem::is_regular_file(path) &&
         !std::filesystem::is_empty(path);
}

void copy_model_into_store(const std::filesystem::path& source, const std::filesystem::path& target)
{
  if (!file_exists_nonempty(source)) {
    throw std::runtime_error("download did not produce model: " + source.string());
  }

  std::filesystem::create_directories(target.parent_path());

  const auto tmp = target.parent_path() / ("." + target.filename().string() + ".tmp");

  try {
    std::filesystem::copy_file(source, tmp, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::rename(tmp, target);
  }
  catch (...) {
    if (std::filesystem::exists(tmp)) {
      platform::safe_delete_file(tmp);
    }
    throw;
  }
}

void run_whisper_model_downloader(const std::string& name)
{
  const auto source_dir = whisper_source_dir();
  const auto downloader = whisper_model_downloader();

  if (!std::filesystem::exists(source_dir / ".git")) {
    throw std::runtime_error("missing whisper.cpp checkout: " + source_dir.string() +
                             ". Run ./scripts/install first.");
  }

  if (!std::filesystem::exists(downloader)) {
    throw std::runtime_error("missing whisper.cpp model downloader: " + downloader.string());
  }

  std::cout << "Downloading model " << name << " via whisper.cpp downloader...\n";

  const std::string script = R"(cd "$1" && bash ./models/download-ggml-model.sh "$2")";
  const bool success = platform::run_process_blocking(
      {"bash", "-c", script, "asryx-model-download", source_dir.string(), name});

  if (!success) {
    throw std::runtime_error("failed to download model " + name);
  }
}

} // namespace

const std::vector<std::string>& get_supported_models()
{
  static const std::vector<std::string> models = {
      "tiny.en", "tiny",     "base.en",  "base",     "small.en",       "small", "medium.en",
      "medium",  "large-v1", "large-v2", "large-v3", "large-v3-turbo", "large"};
  return models;
}

const std::vector<std::string>& get_supported_languages()
{
  static const std::vector<std::string> languages = {
      "en", "zh", "de", "es",  "ru", "ko", "fr", "ja", "pt", "tr", "pl", "ca", "nl", "ar", "sv",
      "it", "id", "hi", "fi",  "vi", "he", "uk", "el", "ms", "cs", "ro", "da", "hu", "ta", "no",
      "th", "ur", "hr", "bg",  "lt", "la", "mi", "ml", "cy", "sk", "te", "fa", "lv", "bn", "sr",
      "az", "sl", "kn", "et",  "mk", "br", "eu", "is", "hy", "ne", "mn", "bs", "kk", "sq", "sw",
      "gl", "mr", "pa", "si",  "km", "sn", "yo", "so", "af", "oc", "ka", "be", "tg", "sd", "gu",
      "am", "yi", "lo", "uz",  "fo", "ht", "ps", "tk", "nn", "mt", "sa", "lb", "my", "bo", "tl",
      "mg", "as", "tt", "haw", "ln", "ha", "ba", "jw", "su", "yue"};
  return languages;
}

std::string get_model_path(const std::string& name)
{
  return (model_dir() / ("ggml-" + name + ".bin")).string();
}

bool is_model_installed(const std::string& name)
{
  return file_exists_nonempty(get_model_path(name));
}

bool is_supported_language(const std::string& language)
{
  if (language == constants::config::auto_language) {
    return true;
  }

  const auto& supported = get_supported_languages();
  return std::find(supported.begin(), supported.end(), language) != supported.end();
}

bool is_english_only_model(const std::string& name)
{
  return name == "tiny.en" || name == "base.en" || name == "small.en" || name == "medium.en";
}

void validate_config(const config::Config& cfg)
{
  if (!is_supported_model_name(cfg.model)) {
    throw std::runtime_error("unsupported model size: " + cfg.model);
  }

  if (!is_supported_language(cfg.language)) {
    throw std::runtime_error("unsupported language: " + cfg.language);
  }

  if (is_english_only_model(cfg.model) && cfg.language != constants::config::auto_language &&
      cfg.language != constants::config::english_language)
  {
    throw std::runtime_error("active model " + cfg.model +
                             " is English-only; use a multilingual model for " + cfg.language);
  }
}

std::string transcription_language_for(const config::Config& cfg)
{
  validate_config(cfg);

  if (is_english_only_model(cfg.model)) {
    return std::string(constants::config::english_language);
  }

  if (cfg.language == constants::config::auto_language) {
    return "";
  }

  return cfg.language;
}

void list_models()
{
  auto cfg = config::load_config();
  std::cout << "Available models:\n";

  for (const auto& m : get_supported_models()) {
    const bool installed = is_model_installed(m);
    const bool active = (m == cfg.model);

    std::cout << "  " << (active ? "* " : "  ") << m << (installed ? " (installed)" : "")
              << (active ? " (active)" : "") << "\n";
  }
}

void install_model(const std::string& name)
{
  if (!is_supported_model_name(name)) {
    throw std::runtime_error("unsupported model size: " + name);
  }

  const auto target_path = std::filesystem::path(get_model_path(name));
  if (file_exists_nonempty(target_path)) {
    std::cout << "Model " << name << " is already installed.\n";
    return;
  }

  const auto downloaded_path = whisper_model_path(name);

  if (!file_exists_nonempty(downloaded_path)) {
    run_whisper_model_downloader(name);
  }
  else {
    std::cout << "Using cached whisper.cpp model: " << downloaded_path << "\n";
  }

  copy_model_into_store(downloaded_path, target_path);

  if (!file_exists_nonempty(target_path)) {
    throw std::runtime_error("model install did not create " + target_path.string());
  }

  std::cout << "Model " << name << " installed successfully.\n";
}

void use_model(const std::string& name)
{
  if (!is_supported_model_name(name)) {
    throw std::runtime_error("unsupported model size: " + name);
  }

  if (!is_model_installed(name)) {
    throw std::runtime_error("model '" + name +
                             "' is not installed. Install it with: asryx --model install " + name);
  }

  auto cfg = config::load_config();
  cfg.model = name;
  validate_config(cfg);
  config::save_config(cfg);

  std::cout << "Using model: " << name << "\n";
}

void use_language(const std::string& language)
{
  auto cfg = config::load_config();
  cfg.language = language;
  validate_config(cfg);
  config::save_config(cfg);

  std::cout << "Using language: " << language << "\n";
}

void uninstall_model(const std::string& name)
{
  if (!is_supported_model_name(name)) {
    throw std::runtime_error("unsupported model size: " + name);
  }

  const auto path = std::filesystem::path(get_model_path(name));

  if (!file_exists_nonempty(path)) {
    std::cout << "Model " << name << " is not installed.\n";
    return;
  }

  const auto cfg = config::load_config();
  if (cfg.model == name) {
    throw std::runtime_error("cannot uninstall active model '" + name +
                             "'; switch models first with: asryx --model use <other>");
  }

  platform::safe_delete_file(path);
  std::cout << "Model " << name << " uninstalled successfully.\n";
}

} // namespace model
