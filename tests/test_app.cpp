#include "app/app.hpp"
#include "config/config.hpp"
#include "constants/constants.hpp"
#include "model/model.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"
#include "runtime/runtime.hpp"
#include "tests/test_helpers.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <vector>

namespace {

std::filesystem::path runtime_file(const std::string& name)
{
  return platform::get_runtime_directory() / name;
}

void clean_runtime_files()
{
  platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_pid_file)));
  platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_wav_file)));
  platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_error_file)));
  platform::safe_delete_file(runtime_file(std::string(constants::runtime::cancel_marker_file)));
  platform::safe_delete_file(runtime_file(std::string(constants::runtime::state_file)));
}

bool recording_files_exist()
{
  return std::filesystem::exists(
             runtime_file(std::string(constants::runtime::recorder_pid_file))) ||
         std::filesystem::exists(
             runtime_file(std::string(constants::runtime::recorder_wav_file))) ||
         std::filesystem::exists(runtime_file(std::string(constants::runtime::state_file)));
}

void write_fake_model()
{
  const std::string default_model(constants::config::default_model);
  std::filesystem::create_directories(
      std::filesystem::path(model::get_model_path(default_model)).parent_path());
  std::ofstream model_file(model::get_model_path(default_model));
  model_file << "fake model content";
  std::ofstream vad_file(model::get_vad_model_path());
  vad_file << "fake VAD model content";
}

void delete_fake_model()
{
  platform::safe_delete_file(model::get_model_path(std::string(constants::config::default_model)));
  platform::safe_delete_file(model::get_vad_model_path());
}

void reset_config()
{
  config::Config cfg;
  cfg.language = std::string(constants::config::english_language);
  config::save_config(cfg);
}

void assert_control_command_does_not_record(const std::vector<std::string>& args)
{
  clean_runtime_files();
  runtime::testing::reset_toggle_entry_count();
  ASSERT_EQ(app::run(args), 0);
  ASSERT_EQ(runtime::testing::toggle_entry_count(), 0);
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));
  ASSERT_FALSE(recording_files_exist());
}

void stop_started_recording()
{
  std::ifstream pid_file(runtime_file(std::string(constants::runtime::recorder_pid_file)));
  pid_t pid = 0;
  pid_file >> pid;

  if (pid > 0) {
    platform::stop_process(pid);
    platform::wait_process(pid);
  }

  clean_runtime_files();
}

} // namespace

void run_test_app()
{
  write_fake_model();
  reset_config();
  clean_runtime_files();

  runtime::testing::reset_toggle_entry_count();
  ASSERT_EQ(app::run({}), 0);
  ASSERT_EQ(runtime::testing::toggle_entry_count(), 1);
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::recording_state));
  ASSERT_TRUE(recording_files_exist());
  stop_started_recording();

  assert_control_command_does_not_record({"status"});
  assert_control_command_does_not_record({"--language", "en"});
  assert_control_command_does_not_record({"--model", "list"});
  assert_control_command_does_not_record({"--model", "install", "base.en"});
  assert_control_command_does_not_record({"--model", "use", "base.en"});
  assert_control_command_does_not_record({"--model", "uninstall", "tiny.en"});
  assert_control_command_does_not_record({"--pipe-to", "tee -a ~/x.txt"});
  assert_control_command_does_not_record({"cancel"});

  auto cfg = config::load_config();
  ASSERT_EQ(cfg.pipe_to, std::string("tee -a ~/x.txt"));

  assert_control_command_does_not_record({"--no-pipe"});
  cfg = config::load_config();
  ASSERT_EQ(cfg.pipe_to, std::string(""));

  clean_runtime_files();
  runtime::testing::reset_toggle_entry_count();
  ASSERT_EQ(app::run({"--output", "clipboard"}), 1);
  ASSERT_EQ(runtime::testing::toggle_entry_count(), 0);
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));
  ASSERT_FALSE(recording_files_exist());

  runtime::testing::reset_toggle_entry_count();
  ASSERT_EQ(app::run({"--output", "exec", "--pipe-to", "tee -a ~/x.txt"}), 1);
  ASSERT_EQ(runtime::testing::toggle_entry_count(), 0);
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));
  ASSERT_FALSE(recording_files_exist());

  delete_fake_model();
  std::cout << "test_app passed\n";
}
