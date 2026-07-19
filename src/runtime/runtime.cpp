#include "runtime/runtime.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "model/model.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"

#include <cctype>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace runtime {

namespace {

#ifdef ASRYX_TESTING
int& _toggle_entries()
{
  static int entries = 0;
  return entries;
}
#endif

std::filesystem::path _lock_dir_for(const std::filesystem::path& runtime_dir)
{
  return runtime_dir / std::string(constants::runtime::lock_dir_name);
}

std::filesystem::path _cancel_marker_for(const std::filesystem::path& runtime_dir)
{
  return runtime_dir / std::string(constants::runtime::cancel_marker_file);
}

bool _read_pid_file(const std::filesystem::path& path, pid_t& pid)
{
  std::ifstream file(path);
  return static_cast<bool>(file >> pid);
}

bool _acquire_lock(const std::filesystem::path& runtime_dir)
{
  std::filesystem::create_directories(runtime_dir);
  const auto lock_dir = _lock_dir_for(runtime_dir);

  std::error_code ec;
  if (std::filesystem::create_directory(lock_dir, ec)) {
    std::ofstream(lock_dir / std::string(constants::runtime::pid_file_name)) << getpid() << "\n";
    return true;
  }

  pid_t pid = 0;
  if (!_read_pid_file(lock_dir / std::string(constants::runtime::pid_file_name), pid) ||
      !platform::is_process_running(pid))
  {
    platform::safe_delete_directory(lock_dir);
    if (std::filesystem::create_directory(lock_dir, ec)) {
      std::ofstream(lock_dir / std::string(constants::runtime::pid_file_name)) << getpid() << "\n";
      return true;
    }
  }

  return false;
}

void _release_lock(const std::filesystem::path& runtime_dir)
{
  platform::safe_delete_directory(_lock_dir_for(runtime_dir));
}

void _clean_stale_payload(const std::filesystem::path& runtime_dir)
{
  platform::safe_delete_file(runtime_dir / std::string(constants::runtime::recorder_pid_file));
  platform::safe_delete_file(runtime_dir / std::string(constants::runtime::recorder_wav_file));
  platform::safe_delete_file(runtime_dir / std::string(constants::runtime::recorder_error_file));
  platform::safe_delete_file(_cancel_marker_for(runtime_dir));
  platform::safe_delete_file(runtime_dir / std::string(constants::runtime::state_file));
}

std::string _read_text_file(const std::filesystem::path& path)
{
  if (!std::filesystem::exists(path)) {
    return "";
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return "";
  }

  std::string output;
  std::string line;

  while (std::getline(file, line)) {
    output += line;
    output += '\n';
  }

  return output;
}

std::string _read_state_file(const std::filesystem::path& runtime_dir)
{
  const auto state_file = runtime_dir / std::string(constants::runtime::state_file);
  if (!std::filesystem::exists(state_file)) {
    return "";
  }

  std::ifstream file(state_file);
  std::string state;
  if (file >> state) {
    return state;
  }

  return "";
}

bool _has_live_lock(const std::filesystem::path& runtime_dir)
{
  pid_t pid = 0;
  return _read_pid_file(_lock_dir_for(runtime_dir) / std::string(constants::runtime::pid_file_name),
                        pid) &&
         platform::is_process_running(pid);
}

bool _has_live_recorder(const std::filesystem::path& runtime_dir, pid_t& pid)
{
  return _read_pid_file(runtime_dir / std::string(constants::runtime::recorder_pid_file), pid) &&
         platform::is_process_running(pid);
}

std::string _trim(std::string value)
{
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.pop_back();
  }

  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }

  return value.substr(start);
}

void _write_state(const std::filesystem::path& runtime_dir, const std::string& state)
{
  std::ofstream file(runtime_dir / std::string(constants::runtime::state_file));
  file << state << "\n";
}

std::string _recorder_error_text(const std::filesystem::path& runtime_dir)
{
  return _trim(_read_text_file(runtime_dir / std::string(constants::runtime::recorder_error_file)));
}

void _print_recorder_error(const std::filesystem::path& runtime_dir)
{
  const auto error = _recorder_error_text(runtime_dir);
  if (!error.empty()) {
    std::cerr << error << "\n";
  }
}

std::filesystem::path _runtime_log_path(const std::filesystem::path& runtime_dir)
{
  return runtime_dir / std::string(constants::runtime::error_log_file);
}

std::filesystem::path _write_runtime_log(const std::filesystem::path& runtime_dir,
                                         const std::string& content)
{
  std::filesystem::create_directories(runtime_dir);
  const auto path = _runtime_log_path(runtime_dir);
  std::ofstream file(path);
  file << content;
  return path;
}

bool _create_cancel_marker(const std::filesystem::path& runtime_dir)
{
  std::filesystem::create_directories(runtime_dir);
  const auto marker_path = _cancel_marker_for(runtime_dir);
  const int fd = open(marker_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd == -1) {
    if (errno == EEXIST) {
      return false;
    }

    throw std::runtime_error("failed to create cancel marker: " + marker_path.string());
  }

  const auto pid = std::to_string(getpid()) + "\n";
  const ssize_t written = write(fd, pid.c_str(), pid.size());
  const int close_status = close(fd);
  if (written < 0 || static_cast<size_t>(written) != pid.size() || close_status == -1) {
    platform::safe_delete_file(marker_path);
    throw std::runtime_error("failed to write cancel marker: " + marker_path.string());
  }

  return true;
}

bool _cancel_requested(const std::filesystem::path& runtime_dir)
{
  return std::filesystem::exists(_cancel_marker_for(runtime_dir));
}

std::string _status_for(const std::filesystem::path& runtime_dir)
{
  const auto state = _read_state_file(runtime_dir);

  pid_t rec_pid = 0;
  if (_has_live_recorder(runtime_dir, rec_pid)) {
    return std::string(constants::runtime::recording_state);
  }

  if (state == constants::runtime::transcribing_state && _has_live_lock(runtime_dir)) {
    return std::string(constants::runtime::transcribing_state);
  }

  return std::string(constants::runtime::idle_state);
}

bool _wait_for_idle(const std::filesystem::path& runtime_dir)
{
  for (int attempt = 0; attempt < 100; ++attempt) {
    if (_status_for(runtime_dir) == constants::runtime::idle_state) {
      return true;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  return _status_for(runtime_dir) == constants::runtime::idle_state;
}

bool _cancel_recording(const std::filesystem::path& runtime_dir)
{
  if (!_acquire_lock(runtime_dir)) {
    return false;
  }

  try {
    pid_t rec_pid = 0;
    if (!_has_live_recorder(runtime_dir, rec_pid)) {
      _release_lock(runtime_dir);
      return true;
    }

    if (!engine::stop_recording(rec_pid)) {
      _print_recorder_error(runtime_dir);
      throw std::runtime_error("recorder did not stop");
    }

    _clean_stale_payload(runtime_dir);
    engine::send_notification(std::string(constants::notifications::cancelled));
    _release_lock(runtime_dir);
    return true;
  }
  catch (...) {
    _release_lock(runtime_dir);
    throw;
  }
}

void _cancel_transcribing(const std::filesystem::path& runtime_dir)
{
  if (!_create_cancel_marker(runtime_dir)) {
    return;
  }

  if (_wait_for_idle(runtime_dir)) {
    return;
  }

  if (_status_for(runtime_dir) == constants::runtime::transcribing_state) {
    engine::send_notification(std::string(constants::notifications::cancelling));
  }
}

void _start_recording(const std::filesystem::path& runtime_dir)
{
  _clean_stale_payload(runtime_dir);

  const auto config = config::load_config();
  model::validate_config(config);
  if (!model::is_model_installed(config.model)) {
    throw std::runtime_error("model '" + config.model +
                             "' is not installed. Install it with: asryx --model install " +
                             config.model);
  }

  model::validate_vad_model();

  const auto wav_path = runtime_dir / std::string(constants::runtime::recorder_wav_file);
  const auto err_path = runtime_dir / std::string(constants::runtime::recorder_error_file);
  const pid_t pid = engine::start_recording(wav_path.string(), err_path.string());
  if (!platform::is_process_running(pid)) {
    throw std::runtime_error("recorder process exited before startup completed");
  }

  std::ofstream(runtime_dir / std::string(constants::runtime::recorder_pid_file)) << pid << "\n";
  _write_state(runtime_dir, std::string(constants::runtime::recording_state));
  engine::send_notification("recording…");
}

void _route_transcription(const std::filesystem::path& runtime_dir, const config::Config& cfg,
                          const std::string& output)
{
  if (!engine::copy_to_clipboard(output)) {
    const auto log_path =
        _write_runtime_log(runtime_dir, "clipboard copy failed! transcript was not copied.\n");
    std::cerr << "clipboard failed! see log: " << log_path << "\n";
    engine::send_notification("clipboard failed! see log");
    _clean_stale_payload(runtime_dir);
    return;
  }

  if (cfg.pipe_to.empty()) {
    engine::send_notification(std::string(constants::notifications::transcription_copied));
    _clean_stale_payload(runtime_dir);
    return;
  }

  if (!platform::run_process_with_stdin({"sh", "-c", cfg.pipe_to}, output)) {
    const auto log_path = _write_runtime_log(
        runtime_dir, "pipe target failed! transcript was copied to clipboard.\n");
    std::cerr << "pipe target failed! transcript remains in clipboard, see log: " << log_path
              << "\n";
    engine::send_notification(std::string(constants::notifications::pipe_failed));
    _clean_stale_payload(runtime_dir);
    return;
  }

  engine::send_notification(std::string(constants::notifications::pipe_copied));
  _clean_stale_payload(runtime_dir);
}

void _stop_and_transcribe(const std::filesystem::path& runtime_dir, pid_t rec_pid)
{
  if (!engine::stop_recording(rec_pid)) {
    _print_recorder_error(runtime_dir);
    engine::send_notification("recorder did not stop");
    return;
  }

  _write_state(runtime_dir, std::string(constants::runtime::transcribing_state));

  const auto config = config::load_config();
  const auto language = model::transcription_language_for(config);
  const auto wav_path = runtime_dir / std::string(constants::runtime::recorder_wav_file);
  const engine::TranscriptionRequest request{.model_path = model::get_model_path(config.model),
                                             .vad_model_path = model::get_vad_model_path(),
                                             .wav_path = wav_path.string(),
                                             .language = language,
                                             .cancel_marker_path =
                                                 _cancel_marker_for(runtime_dir).string()};
  std::string output;
  try {
    output = _trim(engine::transcribe(request));
  }
  catch (const engine::TranscriptionCancelled&) {
    _clean_stale_payload(runtime_dir);
    engine::send_notification(std::string(constants::notifications::cancelled));
    return;
  }
  catch (const std::exception&) {
    if (_cancel_requested(runtime_dir)) {
      _clean_stale_payload(runtime_dir);
      engine::send_notification(std::string(constants::notifications::cancelled));
      return;
    }

    throw;
  }

  if (_cancel_requested(runtime_dir)) {
    _clean_stale_payload(runtime_dir);
    engine::send_notification(std::string(constants::notifications::cancelled));
    return;
  }

  if (output.empty()) {
    engine::send_notification("no output");
    _clean_stale_payload(runtime_dir);
    return;
  }

  _route_transcription(runtime_dir, config, output);
}

} // namespace

std::string get_status()
{
  return _status_for(platform::get_runtime_directory());
}

void cancel()
{
  const auto runtime_dir = platform::get_runtime_directory();
  const auto status = _status_for(runtime_dir);

  if (status == constants::runtime::idle_state) {
    return;
  }

  if (status == constants::runtime::recording_state) {
    if (_cancel_recording(runtime_dir)) {
      return;
    }

    if (_status_for(runtime_dir) == constants::runtime::transcribing_state) {
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
  if (!_acquire_lock(runtime_dir)) {
    return;
  }

  try {
    pid_t rec_pid = 0;
    if (_has_live_recorder(runtime_dir, rec_pid)) {
      _stop_and_transcribe(runtime_dir, rec_pid);
    }
    else {
      _start_recording(runtime_dir);
    }

    _release_lock(runtime_dir);
  }
  catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    const auto recorder_error = _recorder_error_text(runtime_dir);
    if (!recorder_error.empty()) {
      std::cerr << recorder_error << "\n";
    }

    std::ostringstream log;
    log << "error: " << e.what() << "\n";
    if (!recorder_error.empty()) {
      log << recorder_error << "\n";
    }

    const auto log_path = _write_runtime_log(runtime_dir, log.str());
    std::cerr << "see log: " << log_path << "\n";
    engine::send_notification("asryx failed; see log");
    _clean_stale_payload(runtime_dir);
    _release_lock(runtime_dir);
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
