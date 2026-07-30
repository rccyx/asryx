#include "model/store.hpp"

#include "constants/constants.hpp"
#include "platform/fs.hpp"

#include <filesystem>
#include <string>

namespace model::store {

yx::Result<std::filesystem::path> model_dir()
{
  return platform::get_home_relative_path(std::string(constants::paths::models_dir_rel));
}

yx::Result<std::filesystem::path> vad_model_path()
{
  return model_dir().transform([](const std::filesystem::path& dir) {
    return dir / std::string(constants::paths::VAD_MODEL_FILE);
  });
}

yx::Result<std::filesystem::path> whisper_source_dir()
{
  return platform::get_home_relative_path(std::string(constants::paths::whisper_checkout_rel));
}

yx::Result<std::filesystem::path> whisper_model_path(const std::string& name)
{
  return whisper_source_dir().transform([&name](const std::filesystem::path& source_dir) {
    return source_dir / "models" / ("ggml-" + name + ".bin");
  });
}

yx::Result<std::filesystem::path> whisper_model_downloader()
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

yx::Result<void> copy_model_into_store(const std::filesystem::path& source,
                                       const std::filesystem::path& target)
{
  if (!file_exists_nonempty(source)) {
    return yx::fail("download did not produce model: " + source.string());
  }

  std::error_code error;
  std::filesystem::create_directories(target.parent_path(), error);
  if (error) {
    return yx::fail("failed to create model directory: " + error.message());
  }

  const auto tmp = target.parent_path() / ("." + target.filename().string() + ".tmp");

  std::filesystem::copy_file(source, tmp, std::filesystem::copy_options::overwrite_existing, error);
  if (error) {
    return yx::fail("failed to copy model: " + error.message());
  }

  std::filesystem::rename(tmp, target, error);
  if (error) {
    const auto deleted = platform::safe_delete_file(tmp);
    if (!deleted) {
      return yx::fail(deleted.error());
    }

    return yx::fail("failed to store model: " + error.message());
  }

  return yx::ok();
}

} // namespace model::store
