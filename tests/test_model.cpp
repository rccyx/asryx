#include "constants/constants.hpp"
#include "model/model.hpp"
#include "tests/test_helpers.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

void run_test_model()
{
  const auto& supported = model::get_supported_models();
  ASSERT_TRUE(!supported.empty());
  ASSERT_TRUE(std::find(supported.begin(), supported.end(),
                        std::string(constants::config::default_model)) != supported.end());

  std::string path = model::get_model_path(std::string(constants::config::default_model));
  ASSERT_TRUE(!path.empty());

  // Initially it's not installed in test home
  ASSERT_FALSE(model::is_model_installed(std::string(constants::config::default_model)));

  bool missing_install_rejected = false;
  try {
    model::use_model(std::string(constants::config::default_model));
  }
  catch (const std::runtime_error& e) {
    missing_install_rejected =
        std::string(e.what()) ==
        "model 'base.en' is not installed. Install it with: asryx --model install base.en";
  }
  ASSERT_TRUE(missing_install_rejected);

  std::filesystem::create_directories(std::filesystem::path(path).parent_path());
  std::ofstream model_file(path);
  model_file << "fake model content";
  model_file.close();

  model::use_model(std::string(constants::config::default_model));

  bool active_uninstall_rejected = false;
  try {
    model::uninstall_model(std::string(constants::config::default_model));
  }
  catch (const std::runtime_error& e) {
    active_uninstall_rejected =
        std::string(e.what()) ==
        "cannot uninstall active model 'base.en'; switch models first with: asryx --model use "
        "<other>";
  }
  ASSERT_TRUE(active_uninstall_rejected);
  ASSERT_TRUE(std::filesystem::exists(path));

  std::cout << "test_model passed\n";
}
