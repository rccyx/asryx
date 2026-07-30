#include "model/model.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "model/store.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace model {

namespace {

bool _is_supported_model_name(const std::string& name)
{
  const auto& supported_models = get_supported_models();
  return std::find(supported_models.begin(), supported_models.end(), name) !=
         supported_models.end();
}

yx::Result<void> _run_whisper_model_downloader(const std::string& name)
{
  const auto source_dir = store::whisper_source_dir();
  if (!source_dir) {
    return yx::fail(source_dir.error());
  }

  const auto downloader = store::whisper_model_downloader();
  if (!downloader) {
    return yx::fail(downloader.error());
  }

  if (!std::filesystem::exists(*source_dir / ".git")) {
    return yx::fail("missing whisper.cpp checkout: " + source_dir->string() +
                    ". Run ./package/install first.");
  }

  if (!std::filesystem::exists(*downloader)) {
    return yx::fail("missing whisper.cpp model downloader: " + downloader->string());
  }

  std::cout << "Downloading model " << name << " via whisper.cpp downloader...\n";

  const std::string script = R"(cd "$1" && bash ./models/download-ggml-model.sh "$2")";
  const auto success = platform::run_process_blocking(
      {"bash", "-c", script, "asryx-model-download", source_dir->string(), name});
  if (!success) {
    return yx::fail(success.error());
  }

  if (!*success) {
    return yx::fail("failed to download model " + name);
  }

  return yx::ok();
}

} // namespace

yx::Result<std::string> get_model_path(const std::string& name)
{
  return store::model_dir().transform([&name](const std::filesystem::path& model_dir) {
    return (model_dir / ("ggml-" + name + ".bin")).string();
  });
}

yx::Result<std::string> get_vad_model_path()
{
  return store::vad_model_path().transform(
      [](const std::filesystem::path& path) { return path.string(); });
}

yx::Result<bool> is_model_installed(const std::string& name)
{
  return get_model_path(name).transform(
      [](const std::string& path) { return store::file_exists_nonempty(path); });
}

yx::Result<bool> is_vad_model_installed()
{
  return store::vad_model_path().transform(
      [](const std::filesystem::path& path) { return store::file_exists_nonempty(path); });
}

bool is_supported_language(const std::string& language)
{
  if (language == constants::config::auto_language) {
    return true;
  }

  const auto& supported_languages = get_supported_languages();
  return std::find(supported_languages.begin(), supported_languages.end(), language) !=
         supported_languages.end();
}

bool is_english_only_model(const std::string& name)
{
  return name == "tiny.en" || name == "base.en" || name == "small.en" || name == "medium.en";
}

yx::Result<void> validate_config(const config::Config& cfg)
{
  if (!_is_supported_model_name(cfg.model)) {
    return yx::fail("unsupported model size: " + cfg.model);
  }

  if (!is_supported_language(cfg.language)) {
    return yx::fail("unsupported language: " + cfg.language);
  }

  if (is_english_only_model(cfg.model) && cfg.language != constants::config::auto_language &&
      cfg.language != constants::config::english_language)
  {
    return yx::fail("active model " + cfg.model +
                    " is English-only; use a multilingual model for " + cfg.language);
  }

  return yx::ok();
}

yx::Result<void> validate_vad_model()
{
  const auto path = store::vad_model_path();
  if (!path) {
    return yx::fail(path.error());
  }

  if (!store::file_exists_nonempty(*path)) {
    return yx::fail("VAD model is not installed: " + path->string());
  }

  return yx::ok();
}

yx::Result<std::string> transcription_language_for(const config::Config& cfg)
{
  return validate_config(cfg).transform([&cfg] {
    if (is_english_only_model(cfg.model)) {
      return std::string(constants::config::english_language);
    }

    if (cfg.language == constants::config::auto_language) {
      return std::string("");
    }

    return cfg.language;
  });
}

yx::Result<void> list_models()
{
  const auto config = config::load_config();
  if (!config) {
    return yx::fail(config.error());
  }

  std::cout << "Available models:\n";

  for (const auto& model_name : get_supported_models()) {
    const auto installed = is_model_installed(model_name);
    if (!installed) {
      return yx::fail(installed.error());
    }

    const bool active = model_name == config->model;

    std::cout << "  " << (active ? "* " : "  ") << model_name << (*installed ? " (installed)" : "")
              << (active ? " (active)" : "") << "\n";
  }

  std::cout << "\nVAD model:\n";
  const auto vad_installed = is_vad_model_installed();
  if (!vad_installed) {
    return yx::fail(vad_installed.error());
  }

  std::cout << "  " << constants::paths::VAD_MODEL_FILE
            << (*vad_installed ? " (installed)" : " (missing)") << "\n";

  return yx::ok();
}

yx::Result<void> install_model(const std::string& name)
{
  if (!_is_supported_model_name(name)) {
    return yx::fail("unsupported model size: " + name);
  }

  const auto model_path = get_model_path(name);
  if (!model_path) {
    return yx::fail(model_path.error());
  }

  const auto target_path = std::filesystem::path(*model_path);
  if (store::file_exists_nonempty(target_path)) {
    std::cout << "Model " << name << " is already installed.\n";
    return yx::ok();
  }

  const auto downloaded_path = store::whisper_model_path(name);
  if (!downloaded_path) {
    return yx::fail(downloaded_path.error());
  }

  if (!store::file_exists_nonempty(*downloaded_path)) {
    const auto downloaded = _run_whisper_model_downloader(name);
    if (!downloaded) {
      return yx::fail(downloaded.error());
    }
  }
  else {
    std::cout << "Using cached whisper.cpp model: " << *downloaded_path << "\n";
  }

  const auto copied = store::copy_model_into_store(*downloaded_path, target_path);
  if (!copied) {
    return yx::fail(copied.error());
  }

  if (!store::file_exists_nonempty(target_path)) {
    return yx::fail("model install did not create " + target_path.string());
  }

  std::cout << "Model " << name << " installed successfully.\n";
  return yx::ok();
}

yx::Result<void> use_model(const std::string& name)
{
  if (!_is_supported_model_name(name)) {
    return yx::fail("unsupported model size: " + name);
  }

  return is_model_installed(name).and_then([&name](bool installed) -> yx::Result<void> {
    if (!installed) {
      return yx::fail("model '" + name +
                      "' is not installed. Install it with: asryx --model install " + name);
    }

    return config::load_config().and_then([&name](config::Config cfg) {
      cfg.model = name;
      return validate_config(cfg)
          .and_then([&cfg] { return config::save_config(cfg); })
          .transform([&name] { std::cout << "Using model: " << name << "\n"; });
    });
  });
}

yx::Result<void> use_language(const std::string& language)
{
  return config::load_config().and_then([&language](config::Config cfg) {
    cfg.language = language;
    return validate_config(cfg)
        .and_then([&cfg] { return config::save_config(cfg); })
        .transform([&language] { std::cout << "Using language: " << language << "\n"; });
  });
}

yx::Result<void> uninstall_model(const std::string& name)
{
  if (!_is_supported_model_name(name)) {
    return yx::fail("unsupported model size: " + name);
  }

  return get_model_path(name).and_then([&name](const std::string& model_path) {
    const auto path = std::filesystem::path(model_path);

    if (!store::file_exists_nonempty(path)) {
      std::cout << "Model " << name << " is not installed.\n";
      return yx::ok();
    }

    return config::load_config().and_then(
        [&name, path](const config::Config& cfg) -> yx::Result<void> {
          if (cfg.model == name) {
            return yx::fail("cannot uninstall active model '" + name +
                            "'; switch models first with: asryx --model use <other>");
          }

          return platform::safe_delete_file(path).transform(
              [&name] { std::cout << "Model " << name << " uninstalled successfully.\n"; });
        });
  });
}

} // namespace model
