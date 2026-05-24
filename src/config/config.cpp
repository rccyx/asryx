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
  Config cfg;
  auto path = platform::get_home_relative_path(std::string(constants::config::file_name));
  std::ifstream file(path);
  if (!file.is_open()) {
    return cfg;
  }

  std::string line;
  while (std::getline(file, line)) {
    if (line.empty() || line[0] == '#') {
      continue;
    }

    auto pos = line.find('=');
    if (pos == std::string::npos) {
      continue;
    }

    std::string key = line.substr(0, pos);
    std::string val = line.substr(pos + 1);

    if (key == constants::config::model_key) {
      cfg.model = val;
    }
    else if (key == constants::config::language_key) {
      cfg.language = val;
    }
  }

  return cfg;
}

void save_config(const Config& cfg)
{
  auto path = platform::get_home_relative_path(std::string(constants::config::file_name));
  auto tmp_path = platform::get_home_relative_path(std::string(constants::config::file_name) +
                                                   std::string(constants::config::temp_suffix));

  {
    std::ofstream file(tmp_path);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open temporary config file for writing: " +
                               tmp_path.string());
    }

    file << constants::config::model_key << "=" << cfg.model << "\n";
    file << constants::config::language_key << "=" << cfg.language << "\n";
  }

  std::filesystem::rename(tmp_path, path);
}

} // namespace config
