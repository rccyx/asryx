#ifndef ASRYX_TESTS_RUNTIME_CONTEXT_HPP
#define ASRYX_TESTS_RUNTIME_CONTEXT_HPP

#include <filesystem>
#include <string>
#include <sys/types.h>

namespace engine {
struct TranscriptionRequest;
}

namespace runtime_test {

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
  bool stop_result = true;
  bool saw_transcribing_state = false;
  pid_t last_started_pid = 0;
};

TestState& state();

std::filesystem::path runtime_dir();
std::filesystem::path runtime_file(const std::string& name);
std::filesystem::path lock_dir();
std::filesystem::path pipe_output_path();
std::filesystem::path pipe_fail_marker_path();
std::filesystem::path cancel_marker_path();

pid_t dead_pid();
void clean_runtime();
void write_text(const std::filesystem::path& path, const std::string& text);
std::string read_text(const std::filesystem::path& path);
void install_default_hooks();
void write_fake_model();
void reset_config(const std::string& pipe_to = "");
void write_pid_file(pid_t pid);
void write_lock_pid(pid_t pid);
bool runtime_payload_exists();
pid_t read_recorded_pid();
void assert_lock_released();
void write_recording_payload();

} // namespace runtime_test

#endif // ASRYX_TESTS_RUNTIME_CONTEXT_HPP
