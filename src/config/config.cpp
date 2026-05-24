#include "config/config.hpp"

#include "platform/fs.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>

namespace config {

Config load_config()
{
  Config cfg;
  auto path = platform::get_home_relative_path(".asryx.conf");
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

    if (key == "model") {
      cfg.model = val;
    }
    else if (key == "language") {
      cfg.language = val;
    }
  }

  return cfg;
}

void save_config(const Config& cfg)
{
  auto path = platform::get_home_relative_path(".asryx.conf");
  auto tmp_path = platform::get_home_relative_path(".asryx.conf.tmp");

  {
    std::ofstream file(tmp_path);
    if (!file.is_open()) {
      throw std::runtime_error("Failed to open temporary config file for writing: " +
                               tmp_path.string());
    }

    file << "model=" << cfg.model << "\n";
    file << "language=" << cfg.language << "\n";
  }

  std::filesystem::rename(tmp_path, path);
}

} // namespace config