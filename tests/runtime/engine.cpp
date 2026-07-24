#include "engine/engine.hpp"

#include "constants/constants.hpp"
#include "runtime/context.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace engine {

std::expected<pid_t, asryx::Error> start_recording(const std::string& wav_path,
                                                   const std::string& err_path)
{
  auto& state = runtime_test::state();
  ++state.start_calls;
  runtime_test::write_text(wav_path, "fake wav");
  runtime_test::write_text(err_path, "");
  state.last_started_pid = getpid();
  return state.last_started_pid;
}

std::expected<bool, asryx::Error> stop_recording(pid_t pid)
{
  auto& state = runtime_test::state();
  ++state.stop_calls;
  return state.stop_result && pid == getpid();
}

std::expected<std::string, asryx::Error> transcribe(const TranscriptionRequest& request)
{
  auto& state = runtime_test::state();
  ++state.transcribe_calls;
  state.last_cancel_marker_path = request.cancel_marker_path;

  std::ifstream state_file(runtime_test::runtime_file(std::string(constants::runtime::state_file)));
  std::string runtime_state;
  state_file >> runtime_state;
  state.saw_transcribing_state = runtime_state == constants::runtime::transcribing_state;

  if (state.cancel_during_transcribe) {
    runtime_test::write_text(request.cancel_marker_path, "cancel\n");
  }

  return state.transcript;
}

std::expected<bool, asryx::Error> copy_to_clipboard(const std::string& text)
{
  auto& state = runtime_test::state();
  ++state.clipboard_calls;
  state.copied_text = text;
  return state.clipboard_result;
}

std::expected<bool, asryx::Error> send_notification(const std::string& message)
{
  auto& state = runtime_test::state();
  ++state.notification_calls;
  state.last_notification = message;
  return true;
}

} // namespace engine
