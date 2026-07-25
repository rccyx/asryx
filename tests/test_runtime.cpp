#include "constants/constants.hpp"
#include "platform/process.hpp"
#include "runtime/context.hpp"
#include "tests/tests.hpp"

#include <filesystem>
#include <iostream>
#include <libassert/assert.hpp>
#include <string>
#include <unistd.h>

using namespace runtime_test;

namespace {

void test_normal_toggle_flow()
{
  reset_runtime();

  ASSERT(runtime_status() == std::string(constants::runtime::idle_state));

  toggle_runtime();
  ASSERT(state().start_calls == 1);
  ASSERT(state().stop_calls == 0);
  ASSERT(read_recorded_pid() == getpid());
  ASSERT(std::filesystem::exists(runtime_file(std::string(constants::runtime::recorder_pid_file))));
  ASSERT(read_text(runtime_file(std::string(constants::runtime::state_file))) ==
         std::string(constants::runtime::recording_state) + "\n");
  ASSERT(runtime_status() == std::string(constants::runtime::recording_state));
  ASSERT(platform::is_process_running(read_recorded_pid()));
  assert_lock_released();

  toggle_runtime();
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
  reset_runtime();
  write_recording_payload_for(dead_pid());
  write_text(runtime_file(std::string(constants::runtime::recorder_error_file)), "stale err");

  toggle_runtime();

  ASSERT(state().start_calls == 1);
  ASSERT(state().stop_calls == 0);
  ASSERT(read_recorded_pid() == getpid());
  ASSERT(read_text(runtime_file(std::string(constants::runtime::recorder_error_file))).empty());
  assert_lock_released();
}

void test_lock_state()
{
  reset_runtime();

  write_lock_pid(getpid());
  toggle_runtime();
  ASSERT(state().start_calls == 0);
  ASSERT(std::filesystem::exists(lock_dir()));
  delete_lock();

  write_lock_pid(dead_pid());
  toggle_runtime();
  ASSERT(state().start_calls == 1);
  ASSERT(read_recorded_pid() == getpid());
  assert_lock_released();
}

void test_status_from_runtime_state()
{
  reset_runtime();

  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::recording_state) + "\n");
  ASSERT(runtime_status() == std::string(constants::runtime::idle_state));

  clean_runtime();
  write_text(runtime_file(std::string(constants::runtime::state_file)),
             std::string(constants::runtime::transcribing_state) + "\n");
  ASSERT(runtime_status() == std::string(constants::runtime::idle_state));

  write_lock_pid(getpid());
  ASSERT(runtime_status() == std::string(constants::runtime::transcribing_state));
  delete_lock();
}

void test_pipe_delivery()
{
  reset_runtime();
  reset_runtime_with_pipe("cat > " + pipe_output_path().string());
  write_recording_payload();

  toggle_runtime();

  ASSERT(state().copied_text == std::string("transcript text"));
  ASSERT(read_text(pipe_output_path()) == std::string("transcript text\n"));
  ASSERT(state().last_notification == std::string(constants::notifications::pipe_copied));
  ASSERT(state().start_calls == 0);
}

void test_empty_transcription_does_not_route()
{
  reset_runtime();
  reset_runtime_with_pipe("cat > " + pipe_output_path().string());
  state().transcript = " \n\t";
  write_recording_payload();

  toggle_runtime();

  ASSERT(state().clipboard_calls == 0);
  ASSERT(state().last_notification == std::string("no output"));
  ASSERT(!std::filesystem::exists(pipe_output_path()));
}

void test_pipe_failure_keeps_clipboard_copy()
{
  reset_runtime();
  reset_runtime_with_pipe("sh -c 'cat > " + pipe_fail_marker_path().string() + "; exit 7'");
  write_recording_payload();

  toggle_runtime();

  ASSERT(state().copied_text == std::string("transcript text"));
  ASSERT(read_text(pipe_fail_marker_path()) == std::string("transcript text\n"));
  ASSERT(state().last_notification == std::string(constants::notifications::pipe_failed));
  ASSERT(std::filesystem::exists(runtime_file(std::string(constants::runtime::error_log_file))));
  ASSERT(!runtime_payload_exists());
  assert_lock_released();
}

void test_cancel_recording()
{
  reset_runtime();

  cancel_runtime();
  ASSERT(state().stop_calls == 0);
  ASSERT(state().transcribe_calls == 0);
  ASSERT(state().notification_calls == 0);
  ASSERT(!runtime_payload_exists());
  ASSERT(!std::filesystem::exists(lock_dir()));

  write_recording_payload();
  cancel_runtime();
  cancel_runtime();

  ASSERT(state().stop_calls == 1);
  ASSERT(state().transcribe_calls == 0);
  ASSERT(state().notification_calls == 1);
  ASSERT(state().last_notification == std::string(constants::notifications::cancelled));
  ASSERT(!runtime_payload_exists());
  assert_lock_released();
}

void test_cancel_transcribing()
{
  reset_runtime();
  write_transcribing_lock();

  cancel_runtime();
  cancel_runtime();

  ASSERT(state().stop_calls == 0);
  ASSERT(state().transcribe_calls == 0);
  ASSERT(state().notification_calls == 1);
  ASSERT(state().last_notification == std::string(constants::notifications::cancelling));
  ASSERT(std::filesystem::exists(cancel_marker_path()));
  ASSERT(std::filesystem::exists(lock_dir()));
  delete_lock();
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
