#ifndef ASRYX_CONFIG_CONFIG_HPP
#define ASRYX_CONFIG_CONFIG_HPP

#include "constants/constants.hpp"
#include "error.hpp"

#include <expected>
#include <string>

namespace config {

struct Config
{
  std::string model = std::string(constants::config::default_model);
  std::string language = std::string(constants::config::default_language);
  std::string pipe_to;
};

std::expected<Config, asryx::Error> load_config();
std::expected<void, asryx::Error> save_config(const Config& config);

} // namespace config

#endif // ASRYX_CONFIG_CONFIG_HPP
