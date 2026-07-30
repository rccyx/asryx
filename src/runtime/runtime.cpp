#include "runtime/runtime.hpp"

#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "platform/fs.hpp"
#include "runtime/recording/recording.hpp"
#include "runtime/session/session.hpp"
#include "runtime/transcription/transcription.hpp"

#include <filesystem>
#include <sstream>
#include <string>
#include <sys/types.h>

namespace runtime {

namespace {

std::string _notification_for(const yx::Error& error)
{
  if (error.message.find("invalid wav") != std::string::npos ||
      error.message.find("unsupported wav") != std::string::npos)
  {
    return std::string(constants::notifications::audio_parse_failed);
  }

  if (error.message.find("wav file") != std::string::npos ||
      error.message.find("audio file") != std::string::npos)
  {
    return std::string(constants::notifications::audio_read_failed);
  }

  if (error.message.find("recorder") != std::string::npos) {
    return std::string(constants::notifications::recorder_failed);
  }

  if (error.message.find("clipboard") != std::string::npos) {
    return std::string(constants::notifications::clipboard_failed);
  }

  if (error.message.find("pipe") != std::string::npos ||
      error.message.find("process stdin") != std::string::npos)
  {
    return std::string(constants::notifications::pipe_target_failed);
  }

  if (error.message.find("transcription") != std::string::npos ||
      error.message.find("whisper") != std::string::npos ||
      error.message.find("VAD") != std::string::npos)
  {
    return std::string(constants::notifications::transcription_failed);
  }

  return std::string(constants::notifications::runtime_failed);
}

void _handle_runtime_error(const std::filesystem::path& runtime_dir, const yx::Error& error)
{
  const auto recorder_error = session::recorder_error_text(runtime_dir);

  std::ostringstream content;
  content << "error: " << error.message << "\n";
  if (!recorder_error.empty()) {
    content << recorder_error << "\n";
  }

  yx::ignore_failure(session::write_log(runtime_dir, content.str()));
  yx::ignore_failure(engine::send_notification(_notification_for(error)));
  yx::ignore_failure(session::clean_payload(runtime_dir));
  yx::ignore_failure(session::release_lock(runtime_dir));
}

yx::Result<bool> _cancel_recording(const std::filesystem::path& runtime_dir)
{
  const auto acquired = session::acquire_lock(runtime_dir);
  if (!acquired) {
    return yx::fail(acquired.error());
  }

  if (!*acquired) {
    return yx::ok(false);
  }

  const auto rec_pid = session::live_recorder_pid(runtime_dir);
  if (!rec_pid.has_value()) {
    yx::ignore_failure(session::release_lock(runtime_dir));
    return yx::ok(true);
  }

  const auto stopped = engine::stop_recording(*rec_pid);
  if (!stopped) {
    yx::ignore_failure(session::release_lock(runtime_dir));
    return yx::fail(stopped.error());
  }

  if (!*stopped) {
    yx::ignore_failure(session::release_lock(runtime_dir));
    return yx::fail("recorder didn't stop!");
  }

  const auto cleaned = session::clean_payload(runtime_dir);
  if (!cleaned) {
    yx::ignore_failure(session::release_lock(runtime_dir));
    return yx::fail(cleaned.error());
  }

  yx::ignore_failure(engine::send_notification(std::string(constants::notifications::cancelled)));
  const auto released = session::release_lock(runtime_dir);
  if (!released) {
    return yx::fail(released.error());
  }

  return yx::ok(true);
}

yx::Result<void> _cancel_transcribing(const std::filesystem::path& runtime_dir)
{
  const auto marker = session::create_cancel_marker(runtime_dir);
  if (!marker) {
    return yx::fail(marker.error());
  }

  if (!*marker) {
    return yx::ok();
  }

  if (session::wait_for_idle(runtime_dir)) {
    return yx::ok();
  }

  if (session::status_for(runtime_dir) == constants::runtime::transcribing_state) {
    yx::ignore_failure(
        engine::send_notification(std::string(constants::notifications::cancelling)));
  }

  return yx::ok();
}

} // namespace

yx::Result<std::string> get_status()
{
  return yx::ok(session::status_for(platform::get_runtime_directory()));
}

yx::Result<void> cancel()
{
  const auto runtime_dir = platform::get_runtime_directory();
  const auto status = session::status_for(runtime_dir);
  yx::Result<void> result = yx::ok();

  if (status == constants::runtime::idle_state) {
    return yx::ok();
  }

  if (status == constants::runtime::recording_state) {
    const auto cancelled = _cancel_recording(runtime_dir);
    if (!cancelled) {
      _handle_runtime_error(runtime_dir, cancelled.error());
      return yx::fail(cancelled.error());
    }

    if (*cancelled) {
      return yx::ok();
    }

    if (session::status_for(runtime_dir) == constants::runtime::transcribing_state) {
      result = _cancel_transcribing(runtime_dir);
      if (!result) {
        _handle_runtime_error(runtime_dir, result.error());
      }

      return result;
    }

    return yx::ok();
  }

  if (status == constants::runtime::transcribing_state) {
    result = _cancel_transcribing(runtime_dir);
    if (!result) {
      _handle_runtime_error(runtime_dir, result.error());
    }

    return result;
  }

  return yx::ok();
}

yx::Result<void> toggle()
{
  const auto runtime_dir = platform::get_runtime_directory();
  const auto acquired = session::acquire_lock(runtime_dir);
  if (!acquired) {
    _handle_runtime_error(runtime_dir, acquired.error());
    return yx::fail(acquired.error());
  }

  if (!*acquired) {
    return yx::ok();
  }

  const auto rec_pid = session::live_recorder_pid(runtime_dir);
  const auto result = rec_pid.has_value()
                          ? transcription::stop_and_transcribe(runtime_dir, *rec_pid)
                          : recording::start(runtime_dir);
  if (!result) {
    _handle_runtime_error(runtime_dir, result.error());
    return yx::fail(result.error());
  }

  const auto released = session::release_lock(runtime_dir);
  if (!released) {
    _handle_runtime_error(runtime_dir, released.error());
    return yx::fail(released.error());
  }

  return yx::ok();
}

} // namespace runtime
