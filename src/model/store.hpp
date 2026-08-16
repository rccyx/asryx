#ifndef ASRYX_MODEL_STORE_HPP
#define ASRYX_MODEL_STORE_HPP

#include "error.hpp"

#include <filesystem>
#include <string>

namespace model::store {

yx::Result<std::filesystem::path> model_dir();
yx::Result<std::filesystem::path> vad_model_path();
yx::Result<std::filesystem::path> whisper_source_dir();
yx::Result<std::filesystem::path> whisper_model_path(const std::string& name);
yx::Result<std::filesystem::path> whisper_model_downloader();
yx::Result<std::filesystem::path> whisper_model_quantizer();
bool file_exists_nonempty(const std::filesystem::path& path);
yx::Result<void> copy_model_into_store(const std::filesystem::path& source,
                                       const std::filesystem::path& target);

} // namespace model::store

#endif // ASRYX_MODEL_STORE_HPP
