#include "platform/fs.hpp"

#include "constants/constants.hpp"

#include <cstdlib>
#include <stdexcept>
#include <unistd.h>
#include <vector>

namespace platform {

namespace {

std::filesystem::path require_home_path()
{
  const char* home = std::getenv("HOME");
  if (!home || *home == '\0') {
    throw std::runtime_error("HOME environment variable not set");
  }

  return {home};
}

} // namespace

std::filesystem::path get_home_relative_path(const std::string& rel_path)
{
  return require_home_path() / rel_path;
}

std::filesystem::path get_runtime_directory()
{
  const char* xdg = std::getenv("XDG_RUNTIME_DIR");
  if (xdg && *xdg != '\0') {
    return std::filesystem::path(xdg) / std::string(constants::runtime::dir_name);
  }
  return std::filesystem::path(constants::runtime::fallback_tmp_root) /
         (std::string(constants::runtime::dir_name) + "-" + std::to_string(getuid()));
}

bool is_owned_path(const std::filesystem::path& path)
{
  std::filesystem::path canonical_path = std::filesystem::weakly_canonical(path);
  std::filesystem::path home_path;

  try {
    home_path = std::filesystem::weakly_canonical(require_home_path());
  }
  catch (const std::runtime_error&) {
    return false;
  }

  std::vector<std::filesystem::path> allowed = constants::paths::owned_home_paths(home_path);
  allowed.push_back(get_runtime_directory());

  for (const auto& prefix : allowed) {
    std::filesystem::path canonical_prefix = std::filesystem::weakly_canonical(prefix);
    auto rel = canonical_path.lexically_relative(canonical_prefix);
    if (!rel.empty() && rel.string().find("..") == std::string::npos) {
      return true;
    }
  }
  return false;
}

void safe_delete_file(const std::filesystem::path& path)
{
  if (!is_owned_path(path)) {
    throw std::runtime_error("Permission denied: path is not owned by asryx: " + path.string());
  }
  if (std::filesystem::exists(path) || std::filesystem::is_symlink(path)) {
    std::filesystem::remove(path);
  }
}

void safe_delete_directory(const std::filesystem::path& path)
{
  if (!is_owned_path(path)) {
    throw std::runtime_error("Permission denied: path is not owned by asryx: " + path.string());
  }
  if (std::filesystem::exists(path)) {
    std::filesystem::remove_all(path);
  }
}

} // namespace platform