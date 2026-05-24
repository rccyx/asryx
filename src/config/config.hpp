#ifndef ASRYX_CONFIG_CONFIG_HPP
#define ASRYX_CONFIG_CONFIG_HPP

#include "constants/constants.hpp"

#include <string>

namespace config {

struct Config
{
  std::string model = std::string(constants::config::default_model);
  std::string language = std::string(constants::config::default_language);
};

Config load_config();
void save_config(const Config& cfg);

} // namespace config

#endif // ASRYX_CONFIG_CONFIG_HPP
