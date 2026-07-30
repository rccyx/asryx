#include "platform/process.hpp"

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <unistd.h>

namespace platform {

namespace {

bool _is_executable_file(const std::filesystem::path& path)
{
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) && access(path.c_str(), X_OK) == 0;
}

} // namespace

yx::Result<bool> command_exists(const std::string& name)
{
  if (name.empty()) {
    return yx::ok(false);
  }

  if (name.find('/') != std::string::npos) {
    return yx::ok(_is_executable_file(name));
  }

  const char* const path_value = std::getenv("PATH");
  if (path_value == nullptr || *path_value == '\0') {
    return yx::ok(false);
  }

  std::istringstream path_stream(path_value);
  std::string directory;
  while (std::getline(path_stream, directory, ':')) {
    const auto command_path = std::filesystem::path(directory.empty() ? "." : directory) / name;
    if (_is_executable_file(command_path)) {
      return yx::ok(true);
    }
  }

  return yx::ok(false);
}

} // namespace platform
