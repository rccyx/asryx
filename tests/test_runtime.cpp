#include "constants/constants.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"
#include "runtime/context.hpp"
#include "runtime/runtime.hpp"
#include "tests/tests.hpp"

#include <filesystem>
#include <iostream>
#include <libassert/assert.hpp>
#include <string>
#include <unistd.h>

using namespace runtime_test;

namespace {

void _reset_runtime()
{
  write_fake_model();
  reset_config();
  clean_runtime();
}

void _reset_runtime_with_pipe(const std::string& pipe_to)
{
  clean_runtime();
  reset_config(pipe_to);
}

void _write_recording()
{
  write_recording_payload();
}

void _write_recording_for(pid_t pid)
{
  write_pid_file(pid);
  write_text(runtime_file(std::string(constants::runtime::recorder_wav_file)), "fake wav");
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::recording_state) + "\n");
}

void _write_transcribing_lock()
{
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::transcribing_state) + "\n");
  write_lock_pid(getpid());
}

std::string _runtime_status()
{
  const auto status = runtime::get_status();
  ASSERT(status.has_value());
  return *status;
}

void _toggle_runtime()
{
  const auto toggled = runtime::toggle();
  ASSERT(toggled.has_value());
}

void _cancel_runtime()
{
  const auto cancelled = runtime::cancel();
  ASSERT(cancelled.has_value());
}

void _delete_lock()
{
  const auto deleted = platform::safe_delete_directory(lock_dir());
  ASSERT(deleted.has_value());
}

void test_normal_toggle_flow()
{
  _reset_runtime();

  ASSERT(_runtime_status() == std::string(constants::runtime::idle_state));

  _toggle_runtime();
  ASSERT(state().start_calls == 1);
  ASSERT(state().stop_calls == 0);
  ASSERT(read_recorded_pid() == getpid());
  ASSERT(std::filesystem::exists(runtime_file(std::string(constants::runtime::recorder_pid_file))));
  ASSERT(read_text(runtime_file(std::string(constants::runtime::state_file))) ==
         std::string(constants::runtime::recording_state) + "\n");
  ASSERT(_runtime_status() == std::string(constants::runtime::recording_state));
  ASSERT(platform::is_process_running(read_recorded_pid()));
  assert_lock_released();

  _toggle_runtime();
  ASSERT(state().stop_calls == 1);
  ASSERT(state().transcribe_calls == 1);
  ASSERT(state().saw_transcribing_state);
  ASSERT(state().last_cancel_marker_path == cancel_marker_path().string());
  ASSERT(state().clipboard_calls == 1);
  ASSERT(state().copied_text == std::string("transcript text"));
  ASSERT(state().last_notification == std::string(constants::notifications::transcription_copied));
  ASSERT(!runtime_payload_exists());
  assert_lock_released();
}

void test_recovers_dead_recording_pid()
{
  _reset_runtime();
  _write_recording_for(dead_pid());
  write_text(runtime_file(std::string(constants::runtime::recorder_error_file)), "stale err");

  _toggle_runtime();

  ASSERT(state().start_calls == 1);
  ASSERT(state().stop_calls == 0);
  ASSERT(read_recorded_pid() == getpid());
  ASSERT(read_text(runtime_file(std::string(constants::runtime::recorder_error_file))).empty());
  assert_lock_released();
}

void test_lock_state()
{
  _reset_runtime();

  write_lock_pid(getpid());
  _toggle_runtime();
  ASSERT(state().start_calls == 0);
  ASSERT(std::filesystem::exists(lock_dir()));
  _delete_lock();

  write_lock_pid(dead_pid());
  _toggle_runtime();
  ASSERT(state().start_calls == 1);
  ASSERT(read_recorded_pid() == getpid());
  assert_lock_released();
}

void test_status_from_runtime_state()
{
  _reset_runtime();

  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::recording_state) + "\n");
  ASSERT(_runtime_status() == std::string(constants::runtime::idle_state));

  clean_runtime();
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::transcribing_state) + "\n");
  ASSERT(_runtime_status() == std::string(constants::runtime::idle_state));

  write_lock_pid(getpid());
  ASSERT(_runtime_status() == std::string(constants::runtime::transcribing_state));
  _delete_lock();
}

void test_pipe_delivery()
{
  _reset_runtime();
  _reset_runtime_with_pipe("cat > " + pipe_output_path().string());
  _write_recording();

  _toggle_runtime();

  ASSERT(state().copied_text == std::string("transcript text"));
  ASSERT(read_text(pipe_output_path()) == std::string("transcript text\n"));
  ASSERT(state().last_notification == std::string(constants::notifications::pipe_copied));
  ASSERT(state().start_calls == 0);
}

void test_empty_transcription_does_not_route()
{
  _reset_runtime();
  _reset_runtime_with_pipe("cat > " + pipe_output_path().string());
  state().transcript = " \n\t";
  _write_recording();

  _toggle_runtime();

  ASSERT(state().clipboard_calls == 0);
  ASSERT(state().last_notification == std::string("no output"));
  ASSERT(!std::filesystem::exists(pipe_output_path()));
}

void test_pipe_failure_keeps_clipboard_copy()
{
  _reset_runtime();
  _reset_runtime_with_pipe("sh -c 'cat > " + pipe_fail_marker_path().string() + "; exit 7'");
  _write_recording();

  _toggle_runtime();

  ASSERT(state().copied_text == std::string("transcript text"));
  ASSERT(read_text(pipe_fail_marker_path()) == std::string("transcript text\n"));
  ASSERT(state().last_notification == std::string(constants::notifications::pipe_failed));
  ASSERT(std::filesystem::exists(runtime_file(std::string(constants::runtime::error_log_file))));
  ASSERT(!runtime_payload_exists());
  assert_lock_released();
}

void test_cancel_recording()
{
  _reset_runtime();

  _cancel_runtime();
  ASSERT(state().stop_calls == 0);
  ASSERT(state().transcribe_calls == 0);
  ASSERT(state().notification_calls == 0);
  ASSERT(!runtime_payload_exists());
  ASSERT(!std::filesystem::exists(lock_dir()));

  _write_recording();
  _cancel_runtime();
  _cancel_runtime();

  ASSERT(state().stop_calls == 1);
  ASSERT(state().transcribe_calls == 0);
  ASSERT(state().notification_calls == 1);
  ASSERT(state().last_notification == std::string(constants::notifications::cancelled));
  ASSERT(!runtime_payload_exists());
  assert_lock_released();
}

void test_cancel_transcribing()
{
  _reset_runtime();
  _write_transcribing_lock();

  _cancel_runtime();
  _cancel_runtime();

  ASSERT(state().stop_calls == 0);
  ASSERT(state().transcribe_calls == 0);
  ASSERT(state().notification_calls == 1);
  ASSERT(state().last_notification == std::string(constants::notifications::cancelling));
  ASSERT(std::filesystem::exists(cancel_marker_path()));
  ASSERT(std::filesystem::exists(lock_dir()));
  _delete_lock();
}

} // namespace

void run_test_runtime()
{
  test_normal_toggle_flow();
  test_recovers_dead_recording_pid();
  test_lock_state();
  test_status_from_runtime_state();
  test_pipe_delivery();
  test_empty_transcription_does_not_route();
  test_pipe_failure_keeps_clipboard_copy();
  test_cancel_recording();
  test_cancel_transcribing();

  clean_runtime();
  std::cout << "test_runtime passed\n";
}
