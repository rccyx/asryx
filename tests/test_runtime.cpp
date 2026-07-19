#include "config/config.hpp"
#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "model/model.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"
#include "runtime/runtime.hpp"
#include "tests/test_helpers.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace {

struct TestState
{
  int start_calls = 0;
  int stop_calls = 0;
  int transcribe_calls = 0;
  int clipboard_calls = 0;
  int notification_calls = 0;
  std::string copied_text;
  std::string last_notification;
  std::string transcript = " transcript text \n";
  std::string last_cancel_marker_path;
  bool clipboard_result = true;
  bool cancel_during_transcribe = false;
  bool saw_transcribing_state = false;
  pid_t last_started_pid = 0;
};

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

void delete_if_exists(const std::filesystem::path& path)
{
  platform::safe_delete_file(path);
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
  delete_if_exists(pipe_output_path());
  delete_if_exists(pipe_fail_marker_path());
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

pid_t fake_start(const std::string& wav_path, const std::string& err_path)
{
  auto& s = state();
  ++s.start_calls;
  write_text(wav_path, "fake wav");
  write_text(err_path, "");
  s.last_started_pid = getpid();
  return s.last_started_pid;
}

bool fake_stop(pid_t pid)
{
  ++state().stop_calls;
  return pid == getpid();
}

std::string fake_transcribe(const engine::TranscriptionRequest& request)
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

bool fake_clipboard(const std::string& text)
{
  auto& s = state();
  ++s.clipboard_calls;
  s.copied_text = text;
  return s.clipboard_result;
}

bool fake_notify(const std::string& message)
{
  auto& s = state();
  ++s.notification_calls;
  s.last_notification = message;
  return true;
}

void install_default_hooks()
{
  engine::testing::set_start_recording_hook(fake_start);
  engine::testing::set_stop_recording_hook(fake_stop);
  engine::testing::set_transcribe_hook(fake_transcribe);
  engine::testing::set_copy_to_clipboard_hook(fake_clipboard);
  engine::testing::set_notification_hook(fake_notify);
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

void reset_config(const std::string& pipe_to = "")
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

} // namespace

void run_test_runtime()
{
  write_fake_model();
  reset_config();
  clean_runtime();
  install_default_hooks();

  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));

  runtime::toggle();
  ASSERT_EQ(state().start_calls, 1);
  ASSERT_EQ(state().stop_calls, 0);
  ASSERT_EQ(read_recorded_pid(), getpid());
  ASSERT_TRUE(
      std::filesystem::exists(runtime_file(std::string(constants::runtime::recorder_pid_file))));
  ASSERT_EQ(read_text(runtime_file(std::string(constants::runtime::state_file))),
            std::string(constants::runtime::recording_state) + "\n");
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::recording_state));
  ASSERT_TRUE(platform::is_process_running(read_recorded_pid()));
  assert_lock_released();

  runtime::toggle();
  ASSERT_EQ(state().stop_calls, 1);
  ASSERT_EQ(state().transcribe_calls, 1);
  ASSERT_TRUE(state().saw_transcribing_state);
  ASSERT_EQ(state().last_cancel_marker_path, cancel_marker_path().string());
  ASSERT_EQ(state().clipboard_calls, 1);
  ASSERT_EQ(state().copied_text, std::string("transcript text"));
  ASSERT_EQ(state().last_notification, std::string(constants::notifications::transcription_copied));
  ASSERT_FALSE(runtime_payload_exists());
  assert_lock_released();

  clean_runtime();
  install_default_hooks();
  write_pid_file(dead_pid());
  write_text(runtime_file(std::string(constants::runtime::recorder_wav_file)), "stale wav");
  write_text(runtime_file(std::string(constants::runtime::recorder_error_file)), "stale err");
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::recording_state) + "\n");
  runtime::toggle();
  ASSERT_EQ(state().start_calls, 1);
  ASSERT_EQ(state().stop_calls, 0);
  ASSERT_EQ(read_recorded_pid(), getpid());
  ASSERT_TRUE(
      read_text(runtime_file(std::string(constants::runtime::recorder_error_file))).empty());
  assert_lock_released();

  clean_runtime();
  install_default_hooks();
  write_lock_pid(getpid());
  runtime::toggle();
  ASSERT_EQ(state().start_calls, 0);
  ASSERT_TRUE(std::filesystem::exists(lock_dir()));
  platform::safe_delete_directory(lock_dir());

  clean_runtime();
  install_default_hooks();
  write_lock_pid(dead_pid());
  runtime::toggle();
  ASSERT_EQ(state().start_calls, 1);
  ASSERT_EQ(read_recorded_pid(), getpid());
  assert_lock_released();

  clean_runtime();
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::recording_state) + "\n");
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));

  clean_runtime();
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::transcribing_state) + "\n");
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));
  write_lock_pid(getpid());
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::transcribing_state));
  platform::safe_delete_directory(lock_dir());

  clean_runtime();
  reset_config("cat > " + pipe_output_path().string());
  install_default_hooks();
  write_pid_file(getpid());
  write_text(runtime_file(std::string(constants::runtime::recorder_wav_file)), "fake wav");
  runtime::toggle();
  ASSERT_EQ(state().copied_text, std::string("transcript text"));
  ASSERT_EQ(read_text(pipe_output_path()), std::string("transcript text\n"));
  ASSERT_EQ(state().last_notification, std::string(constants::notifications::pipe_copied));
  ASSERT_EQ(state().start_calls, 0);

  clean_runtime();
  reset_config("cat > " + pipe_output_path().string());
  install_default_hooks();
  state().transcript = " \n\t";
  write_pid_file(getpid());
  write_text(runtime_file(std::string(constants::runtime::recorder_wav_file)), "fake wav");
  runtime::toggle();
  ASSERT_EQ(state().clipboard_calls, 0);
  ASSERT_EQ(state().last_notification, std::string("no output"));
  ASSERT_FALSE(std::filesystem::exists(pipe_output_path()));

  clean_runtime();
  reset_config("cat > " + pipe_output_path().string());
  install_default_hooks();
  runtime::toggle();
  ASSERT_EQ(state().start_calls, 1);
  ASSERT_FALSE(std::filesystem::exists(pipe_output_path()));

  clean_runtime();
  reset_config("sh -c 'cat > " + pipe_fail_marker_path().string() + "; exit 7'");
  install_default_hooks();
  write_pid_file(getpid());
  write_text(runtime_file(std::string(constants::runtime::recorder_wav_file)), "fake wav");
  runtime::toggle();
  ASSERT_EQ(state().copied_text, std::string("transcript text"));
  ASSERT_EQ(read_text(pipe_fail_marker_path()), std::string("transcript text\n"));
  ASSERT_EQ(state().last_notification, std::string(constants::notifications::pipe_failed));
  ASSERT_TRUE(
      std::filesystem::exists(runtime_file(std::string(constants::runtime::error_log_file))));
  ASSERT_FALSE(runtime_payload_exists());
  assert_lock_released();

  clean_runtime();
  install_default_hooks();
  write_pid_file(getpid());
  write_text(runtime_file(std::string(constants::runtime::recorder_wav_file)), "fake wav");
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::recording_state) + "\n");
  runtime::cancel();
  ASSERT_EQ(state().stop_calls, 1);
  ASSERT_EQ(state().transcribe_calls, 0);
  ASSERT_EQ(state().last_notification, std::string(constants::notifications::cancelled));
  ASSERT_FALSE(runtime_payload_exists());
  assert_lock_released();

  clean_runtime();
  install_default_hooks();
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::transcribing_state) + "\n");
  write_lock_pid(getpid());
  runtime::cancel();
  ASSERT_EQ(state().stop_calls, 0);
  ASSERT_EQ(state().transcribe_calls, 0);
  ASSERT_EQ(state().last_notification, std::string(constants::notifications::cancelling));
  ASSERT_TRUE(std::filesystem::exists(cancel_marker_path()));
  ASSERT_TRUE(std::filesystem::exists(lock_dir()));
  platform::safe_delete_directory(lock_dir());

  clean_runtime();
  reset_config();
  install_default_hooks();
  state().cancel_during_transcribe = true;
  write_pid_file(getpid());
  write_text(runtime_file(std::string(constants::runtime::recorder_wav_file)), "fake wav");
  runtime::toggle();
  ASSERT_EQ(state().transcribe_calls, 1);
  ASSERT_EQ(state().clipboard_calls, 0);
  ASSERT_EQ(state().last_notification, std::string(constants::notifications::cancelled));
  ASSERT_FALSE(runtime_payload_exists());
  assert_lock_released();

  clean_runtime();
  std::cout << "test_runtime passed\n";
}
