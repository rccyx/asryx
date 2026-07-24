#include "platform/fs.hpp"

#include "constants/constants.hpp"

#include <cstdlib>
#include <unistd.h>
#include <vector>

namespace platform {

namespace {

std::expected<std::filesystem::path, asryx::Error> _require_home_path()
{
  const char* const home = std::getenv("HOME");
  if (!home || *home == '\0') {
    return asryx::fail("HOME environment variable not set");
  }

  return {home};
}

} // namespace

std::expected<std::filesystem::path, asryx::Error> get_home_relative_path(
    const std::string& rel_path)
{
  return _require_home_path().transform(
      [&rel_path](const std::filesystem::path& home) { return home / rel_path; });
}

std::filesystem::path get_runtime_directory()
{
  const char* const runtime_root = std::getenv("XDG_RUNTIME_DIR");
  if (runtime_root && *runtime_root != '\0') {
    return std::filesystem::path(runtime_root) / std::string(constants::runtime::dir_name);
  }

  return std::filesystem::path(constants::runtime::fallback_tmp_root) /
         (std::string(constants::runtime::dir_name) + "-" + std::to_string(getuid()));
}

bool is_owned_path(const std::filesystem::path& path)
{
  std::filesystem::path canonical_path = std::filesystem::weakly_canonical(path);
  const auto home_path = _require_home_path();
  if (!home_path) {
    return false;
  }

  std::vector<std::filesystem::path> allowed =
      constants::paths::owned_home_paths(std::filesystem::weakly_canonical(*home_path));
  allowed.push_back(get_runtime_directory());

  for (const auto& prefix : allowed) {
    const std::filesystem::path canonical_prefix = std::filesystem::weakly_canonical(prefix);
    const auto relative_path = canonical_path.lexically_relative(canonical_prefix);
    if (!relative_path.empty() && relative_path.string().find("..") == std::string::npos) {
      return true;
    }
  }

  return false;
}

std::expected<void, asryx::Error> safe_delete_file(const std::filesystem::path& path)
{
  if (!is_owned_path(path)) {
    return asryx::fail("Permission denied: path is not owned by asryx: " + path.string());
  }
  if (std::filesystem::exists(path) || std::filesystem::is_symlink(path)) {
    std::filesystem::remove(path);
  }

  return {};
}

std::expected<void, asryx::Error> safe_delete_directory(const std::filesystem::path& path)
{
  if (!is_owned_path(path)) {
    return asryx::fail("Permission denied: path is not owned by asryx: " + path.string());
  }
  if (std::filesystem::exists(path)) {
    std::filesystem::remove_all(path);
  }

  return {};
}

} // namespace platform
