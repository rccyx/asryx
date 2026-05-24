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
  model::use_model("base.en");
  std::filesystem::create_directories(
      std::filesystem::path(model::get_model_path("base.en")).parent_path());
  std::ofstream model_file(model::get_model_path("base.en"));
  model_file << "fake model content";

  ASSERT_EQ(runtime::get_status(), std::string("idle"));

  runtime::toggle();
  ASSERT_EQ(runtime::get_status(), std::string("recording"));

  auto runtime_dir = platform::get_runtime_directory();
  std::ifstream pid_file(runtime_dir / "rec.pid");

  pid_t pid = 0;
  pid_file >> pid;
  ASSERT_TRUE(pid > 0);

  platform::stop_process(pid);
  platform::wait_process(pid);

  ASSERT_EQ(runtime::get_status(), std::string("idle"));

  std::cout << "test_runtime passed\n";
}