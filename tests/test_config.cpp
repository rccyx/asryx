#include "config/config.hpp"
#include "constants/constants.hpp"
#include "tests/tests.hpp"

#include <iostream>
#include <libassert/assert.hpp>

void run_test_config()
{
  auto loaded = config::load_config();
  ASSERT(loaded.has_value());
  config::Config cfg = *loaded;
  ASSERT(cfg.model == std::string(constants::config::default_model));
  ASSERT(cfg.language == std::string(constants::config::default_language));
  ASSERT(cfg.pipe_to == std::string(""));

  cfg.model = "small.en";
  cfg.pipe_to = "cat >/dev/null";
  ASSERT(config::save_config(cfg).has_value());

  loaded = config::load_config();
  ASSERT(loaded.has_value());
  config::Config cfg2 = *loaded;
  ASSERT(cfg2.model == std::string("small.en"));
  ASSERT(cfg2.language == std::string(constants::config::default_language));
  ASSERT(cfg2.pipe_to == std::string("cat >/dev/null"));

  std::cout << "test_config passed\n";
}
