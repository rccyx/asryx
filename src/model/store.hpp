#ifndef ASRYX_MODEL_STORE_HPP
#define ASRYX_MODEL_STORE_HPP

#include "error.hpp"

#include <expected>
#include <filesystem>
#include <string>

namespace model::store {

std::expected<std::filesystem::path, asryx::Error> model_dir();
std::expected<std::filesystem::path, asryx::Error> vad_model_path();
std::expected<std::filesystem::path, asryx::Error> whisper_source_dir();
std::expected<std::filesystem::path, asryx::Error> whisper_model_path(const std::string& name);
std::expected<std::filesystem::path, asryx::Error> whisper_model_downloader();
bool file_exists_nonempty(const std::filesystem::path& path);
std::expected<void, asryx::Error> copy_model_into_store(const std::filesystem::path& source,
                                                        const std::filesystem::path& target);

} // namespace model::store

#endif // ASRYX_MODEL_STORE_HPP
