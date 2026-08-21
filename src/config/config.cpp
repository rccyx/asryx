#include "config/config.hpp"

#include "constants/constants.hpp"
#include "platform/fs.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace config {

yx::Result<Config> load_config()
{
  return platform::get_home_relative_path(std::string(constants::config::file_name))
      .transform([](const std::filesystem::path& path) {
        Config config;
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
          else if (key == constants::config::prompt_key) {
            config.prompt = value;
          }
        }

        return config;
      });
}

yx::Result<void> save_config(const Config& config)
{
  const auto path = platform::get_home_relative_path(std::string(constants::config::file_name));
  if (!path) {
    return yx::fail(path.error());
  }

  const auto temp_path = platform::get_home_relative_path(
      std::string(constants::config::file_name) + std::string(constants::config::temp_suffix));
  if (!temp_path) {
    return yx::fail(temp_path.error());
  }

  {
    std::ofstream file(*temp_path);
    if (!file.is_open()) {
      return yx::fail("failed to open tmp conf file for writing: " + temp_path->string());
    }

    file << constants::config::model_key << "=" << config.model << "\n";
    file << constants::config::language_key << "=" << config.language << "\n";
    file << constants::config::pipe_to_key << "=" << config.pipe_to << "\n";
    file << constants::config::prompt_key << "=" << config.prompt << "\n";
    if (!file) {
      return yx::fail("failed to write tmp config file: " + temp_path->string());
    }

    file.flush();
    if (!file) {
      return yx::fail("failed to flush tmp config file: " + temp_path->string());
    }

    file.close();
    if (!file) {
      return yx::fail("failed to close tmp config file: " + temp_path->string());
    }
  }

  std::error_code error;
  std::filesystem::rename(*temp_path, *path, error);
  if (error) {
    return yx::fail("failed to save config file: " + error.message());
  }

  return yx::ok();
}

} // namespace config
