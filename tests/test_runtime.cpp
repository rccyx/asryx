#include "app/app.hpp"
#include "constants/constants.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"
#include "runtime/context.hpp"
#include "runtime/runtime.hpp"
#include "tests/test_helpers.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#include <unistd.h>

using namespace runtime_test;

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
  runtime::cancel();
  ASSERT_EQ(state().stop_calls, 0);
  ASSERT_EQ(state().transcribe_calls, 0);
  ASSERT_EQ(state().notification_calls, 0);
  ASSERT_FALSE(runtime_payload_exists());
  ASSERT_FALSE(std::filesystem::exists(lock_dir()));

  clean_runtime();
  install_default_hooks();
  write_recording_payload();
  runtime::cancel();
  runtime::cancel();
  ASSERT_EQ(state().stop_calls, 1);
  ASSERT_EQ(state().transcribe_calls, 0);
  ASSERT_EQ(state().notification_calls, 1);
  ASSERT_EQ(state().last_notification, std::string(constants::notifications::cancelled));
  ASSERT_FALSE(runtime_payload_exists());
  assert_lock_released();

  clean_runtime();
  install_default_hooks();
  state().stop_result = false;
  write_recording_payload();
  ASSERT_EQ(app::run({"cancel"}), 1);
  ASSERT_EQ(state().stop_calls, 1);
  ASSERT_EQ(state().transcribe_calls, 0);
  ASSERT_EQ(state().clipboard_calls, 0);
  ASSERT_FALSE(state().last_notification == std::string(constants::notifications::cancelled));
  ASSERT_TRUE(runtime_payload_exists());
  assert_lock_released();

  clean_runtime();
  install_default_hooks();
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::transcribing_state) + "\n");
  write_lock_pid(getpid());
  runtime::cancel();
  runtime::cancel();
  ASSERT_EQ(state().stop_calls, 0);
  ASSERT_EQ(state().transcribe_calls, 0);
  ASSERT_EQ(state().notification_calls, 1);
  ASSERT_EQ(state().last_notification, std::string(constants::notifications::cancelling));
  ASSERT_TRUE(std::filesystem::exists(cancel_marker_path()));
  ASSERT_TRUE(std::filesystem::exists(lock_dir()));
  platform::safe_delete_directory(lock_dir());

  clean_runtime();
  install_default_hooks();
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::transcribing_state) + "\n");
  write_lock_pid(getpid());
  std::thread fast_finish([] {
    for (int attempt = 0; attempt < 100; ++attempt) {
      if (std::filesystem::exists(cancel_marker_path())) {
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_pid_file)));
    platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_wav_file)));
    platform::safe_delete_file(runtime_file(std::string(constants::runtime::recorder_error_file)));
    platform::safe_delete_file(cancel_marker_path());
    platform::safe_delete_file(runtime_file(std::string(constants::runtime::state_file)));
    platform::safe_delete_directory(lock_dir());
  });
  runtime::cancel();
  fast_finish.join();
  ASSERT_EQ(state().notification_calls, 0);
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));
  ASSERT_FALSE(runtime_payload_exists());
  assert_lock_released();

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
  reset_config("cat > " + pipe_output_path().string());
  install_default_hooks();
  state().cancel_during_transcribe = true;
  write_pid_file(getpid());
  write_text(runtime_file(std::string(constants::runtime::recorder_wav_file)), "fake wav");
  runtime::toggle();
  ASSERT_EQ(state().transcribe_calls, 1);
  ASSERT_EQ(state().clipboard_calls, 0);
  ASSERT_FALSE(std::filesystem::exists(pipe_output_path()));
  ASSERT_EQ(state().last_notification, std::string(constants::notifications::cancelled));
  ASSERT_FALSE(runtime_payload_exists());
  assert_lock_released();

  clean_runtime();
  std::cout << "test_runtime passed\n";
}
