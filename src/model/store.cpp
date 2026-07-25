#include "model/store.hpp"

#include "constants/constants.hpp"
#include "platform/fs.hpp"

#include <filesystem>
#include <string>

namespace model::store {

std::expected<std::filesystem::path, asryx::Error> model_dir()
{
  return platform::get_home_relative_path(std::string(constants::paths::models_dir_rel));
}

std::expected<std::filesystem::path, asryx::Error> vad_model_path()
{
  return model_dir().transform([](const std::filesystem::path& dir) {
    return dir / std::string(constants::paths::VAD_MODEL_FILE);
  });
}

std::expected<std::filesystem::path, asryx::Error> whisper_source_dir()
{
  return platform::get_home_relative_path(std::string(constants::paths::whisper_checkout_rel));
}

std::expected<std::filesystem::path, asryx::Error> whisper_model_path(const std::string& name)
{
  return whisper_source_dir().transform([&name](const std::filesystem::path& source_dir) {
    return source_dir / "models" / ("ggml-" + name + ".bin");
  });
}

std::expected<std::filesystem::path, asryx::Error> whisper_model_downloader()
{
  return whisper_source_dir().transform([](const std::filesystem::path& source_dir) {
    return source_dir / "models/download-ggml-model.sh";
  });
}

bool file_exists_nonempty(const std::filesystem::path& path)
{
  return std::filesystem::exists(path) && std::filesystem::is_regular_file(path) &&
         !std::filesystem::is_empty(path);
}

std::expected<void, asryx::Error> copy_model_into_store(const std::filesystem::path& source,
                                                        const std::filesystem::path& target)
{
  if (!file_exists_nonempty(source)) {
    return asryx::fail("download did not produce model: " + source.string());
  }

  std::filesystem::create_directories(target.parent_path());

  const auto tmp = target.parent_path() / ("." + target.filename().string() + ".tmp");

  std::error_code error;
  std::filesystem::copy_file(source, tmp, std::filesystem::copy_options::overwrite_existing, error);
  if (error) {
    return asryx::fail("failed to copy model: " + error.message());
  }

  std::filesystem::rename(tmp, target, error);
  if (error) {
    const auto deleted = platform::safe_delete_file(tmp);
    if (!deleted) {
      return std::unexpected(deleted.error());
    }

    return asryx::fail("failed to store model: " + error.message());
  }

  return {};
}

} // namespace model::store
