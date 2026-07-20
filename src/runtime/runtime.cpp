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
#include <stdexcept>
#include <string>
#include <sys/types.h>

namespace runtime {

namespace {

#ifdef ASRYX_TESTING
int& _toggle_entries()
{
  static int entries = 0;
  return entries;
}
#endif

void _handle_toggle_error(const std::filesystem::path& runtime_dir, const std::exception& error)
{
  std::cerr << "error: " << error.what() << "\n";
  const auto recorder_error = session::recorder_error_text(runtime_dir);
  if (!recorder_error.empty()) {
    std::cerr << recorder_error << "\n";
  }

  std::ostringstream content;
  content << "error: " << error.what() << "\n";
  if (!recorder_error.empty()) {
    content << recorder_error << "\n";
  }

  const auto log_path = session::write_log(runtime_dir, content.str());
  std::cerr << "see log: " << log_path << "\n";
  engine::send_notification("failed! see log");
  session::clean_payload(runtime_dir);
  session::release_lock(runtime_dir);
}

bool _cancel_recording(const std::filesystem::path& runtime_dir)
{
  if (!session::acquire_lock(runtime_dir)) {
    return false;
  }

  try {
    pid_t rec_pid = 0;
    if (!session::has_live_recorder(runtime_dir, rec_pid)) {
      session::release_lock(runtime_dir);
      return true;
    }

    if (!engine::stop_recording(rec_pid)) {
      session::print_recorder_error(runtime_dir);
      throw std::runtime_error("recorder didn't stop!");
    }

    session::clean_payload(runtime_dir);
    engine::send_notification(std::string(constants::notifications::cancelled));
    session::release_lock(runtime_dir);
    return true;
  }
  catch (...) {
    session::release_lock(runtime_dir);
    throw;
  }
}

void _cancel_transcribing(const std::filesystem::path& runtime_dir)
{
  if (!session::create_cancel_marker(runtime_dir)) {
    return;
  }

  if (session::wait_for_idle(runtime_dir)) {
    return;
  }

  if (session::status_for(runtime_dir) == constants::runtime::transcribing_state) {
    engine::send_notification(std::string(constants::notifications::cancelling));
  }
}

} // namespace

std::string get_status()
{
  return session::status_for(platform::get_runtime_directory());
}

void cancel()
{
  const auto runtime_dir = platform::get_runtime_directory();
  const auto status = session::status_for(runtime_dir);

  if (status == constants::runtime::idle_state) {
    return;
  }

  if (status == constants::runtime::recording_state) {
    if (_cancel_recording(runtime_dir)) {
      return;
    }

    if (session::status_for(runtime_dir) == constants::runtime::transcribing_state) {
      _cancel_transcribing(runtime_dir);
    }

    return;
  }

  if (status == constants::runtime::transcribing_state) {
    _cancel_transcribing(runtime_dir);
  }
}

void toggle()
{
#ifdef ASRYX_TESTING
  ++_toggle_entries();
#endif

  const auto runtime_dir = platform::get_runtime_directory();
  if (!session::acquire_lock(runtime_dir)) {
    return;
  }

  try {
    pid_t rec_pid = 0;
    if (session::has_live_recorder(runtime_dir, rec_pid)) {
      transcription::stop_and_transcribe(runtime_dir, rec_pid);
    }
    else {
      recording::start(runtime_dir);
    }

    session::release_lock(runtime_dir);
  }
  catch (const std::exception& e) {
    _handle_toggle_error(runtime_dir, e);
    throw;
  }
}

#ifdef ASRYX_TESTING
namespace testing {

void reset_toggle_entry_count()
{
  _toggle_entries() = 0;
}

int toggle_entry_count()
{
  return _toggle_entries();
}

} // namespace testing
#endif

} // namespace runtime
