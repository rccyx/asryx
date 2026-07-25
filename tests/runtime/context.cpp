#include "runtime/context.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "error.hpp"
#include "platform/fs.hpp"
#include "runtime/runtime.hpp"
#include "tests/model_store.hpp"

#include <filesystem>
#include <fstream>
#include <libassert/assert.hpp>
#include <memory>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace runtime_test {

namespace {

void _delete_if_exists(const std::filesystem::path& path)
{
  asryx::ignore_failure(platform::safe_delete_file(path));
}

} // namespace

TestState& state()
{
  static std::unique_ptr<TestState> value;
  if (!value) {
    value = std::make_unique<TestState>();
  }

  return *value;
}

std::filesystem::path runtime_dir()
{
  return platform::get_runtime_directory();
}

std::filesystem::path runtime_file(const std::string& name)
{
  return runtime_dir() / name;
}

std::filesystem::path lock_dir()
{
  return runtime_dir() / std::string(constants::runtime::lock_dir_name);
}

std::filesystem::path pipe_output_path()
{
  return runtime_dir() / "pipe.out";
}

std::filesystem::path pipe_fail_marker_path()
{
  return runtime_dir() / "pipe-failed.out";
}

std::filesystem::path cancel_marker_path()
{
  return runtime_file(std::string(constants::runtime::cancel_marker_file));
}

pid_t dead_pid()
{
  return 99999999;
}

void clean_runtime()
{
  asryx::ignore_failure(platform::safe_delete_directory(lock_dir()));
  asryx::ignore_failure(
      platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_pid_file))));
  asryx::ignore_failure(
      platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_wav_file))));
  asryx::ignore_failure(platform::safe_delete_file(
      runtime_file(std::string(constants::runtime::recorder_error_file))));
  asryx::ignore_failure(platform::safe_delete_file(cancel_marker_path()));
  asryx::ignore_failure(
      platform::safe_delete_file(runtime_file(std::string(constants::runtime::state_file))));
  asryx::ignore_failure(
      platform::safe_delete_file(runtime_file(std::string(constants::runtime::error_log_file))));
  _delete_if_exists(pipe_output_path());
  _delete_if_exists(pipe_fail_marker_path());
  state() = TestState{};
}

void reset_runtime()
{
  write_fake_model();
  reset_config();
  clean_runtime();
}

void reset_runtime_with_pipe(const std::string& pipe_to)
{
  clean_runtime();
  reset_config(pipe_to);
}

void write_text(const std::filesystem::path& path, const std::string& text)
{
  std::filesystem::create_directories(path.parent_path());
  std::ofstream file(path);
  file << text;
}

std::string read_text(const std::filesystem::path& path)
{
  std::ifstream file(path);
  std::string output;
  std::string line;
  while (std::getline(file, line)) {
    output += line;
    output += '\n';
  }
  return output;
}

void write_fake_model()
{
  model_store::write_default_model_and_vad();
}

void reset_config(const std::string& pipe_to)
{
  config::Config cfg;
  cfg.language = std::string(constants::config::english_language);
  cfg.pipe_to = pipe_to;
  ASSERT(config::save_config(cfg).has_value());
}

void write_pid_file(pid_t pid)
{
  write_text(runtime_file(std::string(constants::runtime::recorder_pid_file)),
             std::to_string(pid) + "\n");
}

void write_lock_pid(pid_t pid)
{
  std::filesystem::create_directories(lock_dir());
  write_text(lock_dir() / std::string(constants::runtime::pid_file_name),
             std::to_string(pid) + "\n");
}

bool runtime_payload_exists()
{
  return std::filesystem::exists(
             runtime_file(std::string(constants::runtime::recorder_pid_file))) ||
         std::filesystem::exists(
             runtime_file(std::string(constants::runtime::recorder_wav_file))) ||
         std::filesystem::exists(
             runtime_file(std::string(constants::runtime::recorder_error_file))) ||
         std::filesystem::exists(cancel_marker_path()) ||
         std::filesystem::exists(runtime_file(std::string(constants::runtime::state_file)));
}

pid_t read_recorded_pid()
{
  std::ifstream file(runtime_file(std::string(constants::runtime::recorder_pid_file)));
  pid_t pid = 0;
  file >> pid;
  return pid;
}

std::string runtime_status()
{
  const auto status = runtime::get_status();
  ASSERT(status.has_value());
  return *status;
}

void toggle_runtime()
{
  const auto toggled = runtime::toggle();
  ASSERT(toggled.has_value());
}

void cancel_runtime()
{
  const auto cancelled = runtime::cancel();
  ASSERT(cancelled.has_value());
}

void delete_lock()
{
  const auto deleted = platform::safe_delete_directory(lock_dir());
  ASSERT(deleted.has_value());
}

void assert_lock_released()
{
  ASSERT(!std::filesystem::exists(lock_dir()));
}

void write_recording_payload()
{
  write_recording_payload_for(getpid());
}

void write_recording_payload_for(pid_t pid)
{
  write_pid_file(pid);
  write_text(runtime_file(std::string(constants::runtime::recorder_wav_file)), "fake wav");
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::recording_state) + "\n");
}

void write_transcribing_lock()
{
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::transcribing_state) + "\n");
  write_lock_pid(getpid());
}

} // namespace runtime_test
