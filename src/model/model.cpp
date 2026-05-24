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

std::string get_model_path(const std::string& name)
{
  return (model_dir() / ("ggml-" + name + ".bin")).string();
}

bool is_model_installed(const std::string& name)
{
  return file_exists_nonempty(get_model_path(name));
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
  const auto& supported = get_supported_models();

  if (std::find(supported.begin(), supported.end(), name) == supported.end()) {
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
  const auto& supported = get_supported_models();

  if (std::find(supported.begin(), supported.end(), name) == supported.end()) {
    throw std::runtime_error("unsupported model size: " + name);
  }

  auto cfg = config::load_config();
  cfg.model = name;
  config::save_config(cfg);

  std::cout << "Using model: " << name << "\n";
}

void uninstall_model(const std::string& name)
{
  const auto path = std::filesystem::path(get_model_path(name));

  if (!std::filesystem::exists(path)) {
    std::cout << "Model " << name << " is not installed.\n";
    return;
  }

  platform::safe_delete_file(path);
  std::cout << "Model " << name << " uninstalled successfully.\n";
}

} // namespace model
