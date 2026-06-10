#include "config/config.hpp"

#include "constants/constants.hpp"
#include "platform/fs.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace config {

Config load_config()
{
  Config config;
  const auto path = platform::get_home_relative_path(std::string(constants::config::file_name));
  std::ifstream file(path);
  if (!file.is_open()) {
    return config;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    const auto pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }

    const std::string key = line.substr(0, pos);
    const std::string value = line.substr(pos + 1);

    if (key == constants::config::model_key) {
      config.model = value;
    }
    else if (key == constants::config::language_key) {
      config.language = value;
    }
    else if (key == constants::config::pipe_to_key) {
      config.pipe_to = value;
    }
  }

  return config;
}

void save_config(const Config& config)
{
  const auto path = platform::get_home_relative_path(std::string(constants::config::file_name));
  const auto temp_path = platform::get_home_relative_path(
      std::string(constants::config::file_name) + std::string(constants::config::temp_suffix));

  {
    std::ofstream file(temp_path);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open temporary config file for writing: " +
                               temp_path.string());
    }

    file << constants::config::model_key << "=" << config.model << "\n";
    file << constants::config::language_key << "=" << config.language << "\n";
    file << constants::config::pipe_to_key << "=" << config.pipe_to << "\n";
  }

  std::filesystem::rename(temp_path, path);
}

} // namespace config
