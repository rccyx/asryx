#include "runtime/runtime.hpp"

#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "platform/fs.hpp"
#include "runtime/recording/recording.hpp"
#include "runtime/session/session.hpp"
#include "runtime/transcription/transcription.hpp"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>

namespace runtime {

namespace {

void _handle_toggle_error(const std::filesystem::path& runtime_dir, const asryx::Error& error)
{
  std::cerr << "error: " << error.message << "\n";
  const auto recorder_error = session::recorder_error_text(runtime_dir);
  if (!recorder_error.empty()) {
    std::cerr << recorder_error << "\n";
  }

  std::ostringstream content;
  content << "error: " << error.message << "\n";
  if (!recorder_error.empty()) {
    content << recorder_error << "\n";
  }

  const auto log_path = session::write_log(runtime_dir, content.str());
  std::cerr << "see log: " << log_path << "\n";
  asryx::ignore_failure(engine::send_notification("failed! see log"));
  asryx::ignore_failure(session::clean_payload(runtime_dir));
  asryx::ignore_failure(session::release_lock(runtime_dir));
}

std::expected<bool, asryx::Error> _cancel_recording(const std::filesystem::path& runtime_dir)
{
  const auto acquired = session::acquire_lock(runtime_dir);
  if (!acquired) {
    return std::unexpected(acquired.error());
  }

  if (!*acquired) {
    return false;
  }

  const auto rec_pid = session::live_recorder_pid(runtime_dir);
  if (!rec_pid.has_value()) {
    asryx::ignore_failure(session::release_lock(runtime_dir));
    return true;
  }

  const auto stopped = engine::stop_recording(*rec_pid);
  if (!stopped) {
    asryx::ignore_failure(session::release_lock(runtime_dir));
    return std::unexpected(stopped.error());
  }

  if (!*stopped) {
    session::print_recorder_error(runtime_dir);
    asryx::ignore_failure(session::release_lock(runtime_dir));
    return asryx::fail("recorder didn't stop!");
  }

  const auto cleaned = session::clean_payload(runtime_dir);
  if (!cleaned) {
    asryx::ignore_failure(session::release_lock(runtime_dir));
    return std::unexpected(cleaned.error());
  }

  asryx::ignore_failure(
      engine::send_notification(std::string(constants::notifications::cancelled)));
  const auto released = session::release_lock(runtime_dir);
  if (!released) {
    return std::unexpected(released.error());
  }

  return true;
}

std::expected<void, asryx::Error> _cancel_transcribing(const std::filesystem::path& runtime_dir)
{
  const auto marker = session::create_cancel_marker(runtime_dir);
  if (!marker) {
    return std::unexpected(marker.error());
  }

  if (!*marker) {
    return {};
  }

  if (session::wait_for_idle(runtime_dir)) {
    return {};
  }

  if (session::status_for(runtime_dir) == constants::runtime::transcribing_state) {
    asryx::ignore_failure(
        engine::send_notification(std::string(constants::notifications::cancelling)));
  }

  return {};
}

} // namespace

std::expected<std::string, asryx::Error> get_status()
{
  return session::status_for(platform::get_runtime_directory());
}

std::expected<void, asryx::Error> cancel()
{
  const auto runtime_dir = platform::get_runtime_directory();
  const auto status = session::status_for(runtime_dir);

  if (status == constants::runtime::idle_state) {
    return {};
  }

  if (status == constants::runtime::recording_state) {
    const auto cancelled = _cancel_recording(runtime_dir);
    if (!cancelled) {
      return std::unexpected(cancelled.error());
    }

    if (*cancelled) {
      return {};
    }

    if (session::status_for(runtime_dir) == constants::runtime::transcribing_state) {
      return _cancel_transcribing(runtime_dir);
    }

    return {};
  }

  if (status == constants::runtime::transcribing_state) {
    return _cancel_transcribing(runtime_dir);
  }

  return {};
}

std::expected<void, asryx::Error> toggle()
{
  const auto runtime_dir = platform::get_runtime_directory();
  const auto acquired = session::acquire_lock(runtime_dir);
  if (!acquired) {
    return std::unexpected(acquired.error());
  }

  if (!*acquired) {
    return {};
  }

  const auto rec_pid = session::live_recorder_pid(runtime_dir);
  const auto result = rec_pid.has_value()
                          ? transcription::stop_and_transcribe(runtime_dir, *rec_pid)
                          : recording::start(runtime_dir);
  if (!result) {
    _handle_toggle_error(runtime_dir, result.error());
    return std::unexpected(result.error());
  }

  return session::release_lock(runtime_dir);
}

} // namespace runtime
