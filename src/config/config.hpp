#ifndef ASRYX_CONFIG_CONFIG_HPP
#define ASRYX_CONFIG_CONFIG_HPP

#include <string>

namespace config {

struct Config
{
  std::string model = "base.en";
  std::string language = "auto";
};

Config load_config();
void save_config(const Config& cfg);

} // namespace config

#endif // ASRYX_CONFIG_CONFIG_HPP