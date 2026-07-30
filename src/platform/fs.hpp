#ifndef ASRYX_PLATFORM_FS_HPP
#define ASRYX_PLATFORM_FS_HPP

#include "error.hpp"

#include <filesystem>
#include <string>

namespace platform {

yx::Result<std::filesystem::path> get_home_relative_path(const std::string& rel_path);
std::filesystem::path get_runtime_directory();
bool is_owned_path(const std::filesystem::path& path);
yx::Result<void> safe_delete_file(const std::filesystem::path& path);
yx::Result<void> safe_delete_directory(const std::filesystem::path& path);

} // namespace platform

#endif // ASRYX_PLATFORM_FS_HPP
