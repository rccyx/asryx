#include "app/app.hpp"
#include "config/config.hpp"
#include "constants/constants.hpp"
#include "error.hpp"
#include "platform/fs.hpp"
#include "runtime/runtime.hpp"
#include "runtime/stream/stream.hpp"
#include "tests/model_store.hpp"
#include "tests/tests.hpp"

#include <filesystem>
#include <iostream>
#include <libassert/assert.hpp>
#include <span>
#include <string>
#include <vector>

namespace {

std::filesystem::path runtime_file(const std::string& name)
{
  return platform::get_runtime_directory() / name;
}

void clean_runtime_files()
{
  yx::ignore_failure(
      platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_pid_file))));
  yx::ignore_failure(
      platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_wav_file))));
  yx::ignore_failure(platform::safe_delete_file(
      runtime_file(std::string(constants::runtime::recorder_error_file))));
  yx::ignore_failure(platform::safe_delete_file(
      runtime_file(std::string(constants::runtime::cancel_marker_file))));
  yx::ignore_failure(
      platform::safe_delete_file(runtime_file(std::string(constants::runtime::state_file))));
}

bool recording_files_exist()
{
  return std::filesystem::exists(
             runtime_file(std::string(constants::runtime::recorder_pid_file))) ||
         std::filesystem::exists(
             runtime_file(std::string(constants::runtime::recorder_wav_file))) ||
         std::filesystem::exists(runtime_file(std::string(constants::runtime::state_file)));
}

void reset_config()
{
  config::Config cfg;
  cfg.language = std::string(constants::config::english_language);
  ASSERT(config::save_config(cfg).has_value());
}

void assert_control_command_does_not_record(const std::vector<std::string>& args)
{
  clean_runtime_files();
  const auto exit_code = app::run(args);
  ASSERT(exit_code.has_value());
  ASSERT(*exit_code == 0);

  const auto status = runtime::get_status();
  ASSERT(status.has_value());
  ASSERT(*status == std::string(constants::runtime::idle_state));
  ASSERT(!recording_files_exist());
}

void assert_control_commands_do_not_record(std::span<const std::vector<std::string>> commands)
{
  for (const auto& command : commands) {
    assert_control_command_does_not_record(command);
  }
}

} // namespace

void run_test_app()
{
  model_store::write_default_model_and_vad();
  reset_config();
  clean_runtime_files();

  auto exit_code = app::run({});
  ASSERT(exit_code.has_value());
  ASSERT(*exit_code == 0);

  auto status = runtime::get_status();
  ASSERT(status.has_value());
  ASSERT(*status == std::string(constants::runtime::recording_state));
  ASSERT(recording_files_exist());
  ASSERT(runtime::stream::active(platform::get_runtime_directory()));
  ASSERT(app::run({"cancel"}).value_or(1) == 0);
  ASSERT(!runtime::stream::active(platform::get_runtime_directory()));

  const std::vector<std::vector<std::string>> control_commands = {
      {"status"},
      {"--language", "en"},
      {"--model", "list"},
      {"--model", "install", "base.en"},
      {"--model", "use", "base.en"},
      {"--model", "uninstall", "tiny.en"},
      {"--pipe-to", "tee -a ~/x.txt"},
      {"--prompt", "Project Atlas, Kubernetes, PostgreSQL, OAuth"},
      {"cancel"},
  };
  assert_control_commands_do_not_record(control_commands);

  auto loaded_config = config::load_config();
  ASSERT(loaded_config.has_value());
  auto cfg = *loaded_config;
  ASSERT(cfg.pipe_to == std::string("tee -a ~/x.txt"));
  ASSERT(cfg.prompt == std::string("Project Atlas, Kubernetes, PostgreSQL, OAuth"));

  assert_control_command_does_not_record({"--no-pipe"});
  loaded_config = config::load_config();
  ASSERT(loaded_config.has_value());
  cfg = *loaded_config;
  ASSERT(cfg.pipe_to == std::string(""));
  ASSERT(cfg.prompt == std::string("Project Atlas, Kubernetes, PostgreSQL, OAuth"));

  assert_control_command_does_not_record({"--no-prompt"});
  loaded_config = config::load_config();
  ASSERT(loaded_config.has_value());
  cfg = *loaded_config;
  ASSERT(cfg.prompt == std::string(""));

  clean_runtime_files();
  exit_code = app::run({"--output", "clipboard"});
  ASSERT(exit_code.has_value());
  ASSERT(*exit_code == 1);

  status = runtime::get_status();
  ASSERT(status.has_value());
  ASSERT(*status == std::string(constants::runtime::idle_state));
  ASSERT(!recording_files_exist());

  exit_code = app::run({"--output", "exec", "--pipe-to", "tee -a ~/x.txt"});
  ASSERT(exit_code.has_value());
  ASSERT(*exit_code == 1);

  status = runtime::get_status();
  ASSERT(status.has_value());
  ASSERT(*status == std::string(constants::runtime::idle_state));
  ASSERT(!recording_files_exist());

  model_store::delete_default_model_and_vad();
  std::cout << "test_app passed\n";
}
