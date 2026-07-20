#include "runtime/context.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "model/model.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"
#include "tests/test_helpers.hpp"

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace runtime_test {

namespace {

void _delete_if_exists(const std::filesystem::path& path)
{
  platform::safe_delete_file(path);
}

pid_t _fake_start(const std::string& wav_path, const std::string& err_path)
{
  auto& s = state();
  ++s.start_calls;
  write_text(wav_path, "fake wav");
  write_text(err_path, "");
  s.last_started_pid = getpid();
  return s.last_started_pid;
}

bool _fake_stop(pid_t pid)
{
  ++state().stop_calls;
  return state().stop_result && pid == getpid();
}

std::string _fake_transcribe(const engine::TranscriptionRequest& request)
{
  auto& s = state();
  ++s.transcribe_calls;
  s.last_cancel_marker_path = request.cancel_marker_path;

  std::ifstream state_file(runtime_file(std::string(constants::runtime::state_file)));
  std::string runtime_state;
  state_file >> runtime_state;
  s.saw_transcribing_state = runtime_state == constants::runtime::transcribing_state;

  if (s.cancel_during_transcribe) {
    write_text(request.cancel_marker_path, "cancel\n");
  }

  return s.transcript;
}

bool _fake_clipboard(const std::string& text)
{
  auto& s = state();
  ++s.clipboard_calls;
  s.copied_text = text;
  return s.clipboard_result;
}

bool _fake_notify(const std::string& message)
{
  auto& s = state();
  ++s.notification_calls;
  s.last_notification = message;
  return true;
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
  engine::testing::reset_hooks();
  platform::safe_delete_directory(lock_dir());
  platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_pid_file)));
  platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_wav_file)));
  platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_error_file)));
  platform::safe_delete_file(cancel_marker_path());
  platform::safe_delete_file(runtime_file(std::string(constants::runtime::state_file)));
  platform::safe_delete_file(runtime_file(std::string(constants::runtime::error_log_file)));
  _delete_if_exists(pipe_output_path());
  _delete_if_exists(pipe_fail_marker_path());
  state() = TestState{};
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

void install_default_hooks()
{
  engine::testing::set_start_recording_hook(_fake_start);
  engine::testing::set_stop_recording_hook(_fake_stop);
  engine::testing::set_transcribe_hook(_fake_transcribe);
  engine::testing::set_copy_to_clipboard_hook(_fake_clipboard);
  engine::testing::set_notification_hook(_fake_notify);
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

void reset_config(const std::string& pipe_to)
{
  config::Config cfg;
  cfg.language = std::string(constants::config::english_language);
  cfg.pipe_to = pipe_to;
  config::save_config(cfg);
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

void assert_lock_released()
{
  ASSERT_FALSE(std::filesystem::exists(lock_dir()));
}

void write_recording_payload()
{
  write_pid_file(getpid());
  write_text(runtime_file(std::string(constants::runtime::recorder_wav_file)), "fake wav");
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::recording_state) + "\n");
}

} // namespace runtime_test
