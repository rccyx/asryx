#include "constants/constants.hpp"
#include "model/model.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"
#include "runtime/runtime.hpp"
#include "tests/test_helpers.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sys/types.h>

void run_test_runtime()
{
  const std::string default_model(constants::config::default_model);
  model::use_model(default_model);
  std::filesystem::create_directories(
      std::filesystem::path(model::get_model_path(default_model)).parent_path());
  std::ofstream model_file(model::get_model_path(default_model));
  model_file << "fake model content";

  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));

  runtime::toggle();
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::recording_state));

  auto runtime_dir = platform::get_runtime_directory();
  std::ifstream pid_file(runtime_dir / std::string(constants::runtime::recorder_pid_file));

  pid_t pid = 0;
  pid_file >> pid;
  ASSERT_TRUE(pid > 0);

  platform::stop_process(pid);
  platform::wait_process(pid);

  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));

  std::cout << "test_runtime passed\n";
}
