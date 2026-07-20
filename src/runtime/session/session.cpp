#include "runtime/session/session.hpp"

#include "constants/constants.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"

#include <cctype>
#include <cerrno>
#include <chrono>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unistd.h>

namespace runtime::session {

namespace {

std::filesystem::path _lock_dir_path(const std::filesystem::path& runtime_dir)
{
  return runtime_dir / std::string(constants::runtime::lock_dir_name);
}

std::filesystem::path _state_path(const std::filesystem::path& runtime_dir)
{
  return runtime_dir / std::string(constants::runtime::state_file);
}

bool _read_pid_file(const std::filesystem::path& path, pid_t& pid)
{
  std::ifstream file(path);
  return static_cast<bool>(file >> pid);
}

void _write_pid(const std::filesystem::path& lock_dir)
{
  std::ofstream(lock_dir / std::string(constants::runtime::pid_file_name)) << getpid() << "\n";
}

bool _has_live_lock(const std::filesystem::path& runtime_dir)
{
  pid_t pid = 0;
  return _read_pid_file(
             _lock_dir_path(runtime_dir) / std::string(constants::runtime::pid_file_name), pid) &&
         platform::is_process_running(pid);
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

std::string _read_state(const std::filesystem::path& runtime_dir)
{
  const auto state_file = _state_path(runtime_dir);
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

} // namespace

std::filesystem::path cancel_marker_path(const std::filesystem::path& runtime_dir)
{
  return runtime_dir / std::string(constants::runtime::cancel_marker_file);
}

std::filesystem::path recorder_wav_path(const std::filesystem::path& runtime_dir)
{
  return runtime_dir / std::string(constants::runtime::recorder_wav_file);
}

std::filesystem::path recorder_error_path(const std::filesystem::path& runtime_dir)
{
  return runtime_dir / std::string(constants::runtime::recorder_error_file);
}

std::filesystem::path recorder_pid_path(const std::filesystem::path& runtime_dir)
{
  return runtime_dir / std::string(constants::runtime::recorder_pid_file);
}

bool acquire_lock(const std::filesystem::path& runtime_dir)
{
  std::filesystem::create_directories(runtime_dir);
  const auto lock_dir = _lock_dir_path(runtime_dir);

  std::error_code ec;
  if (std::filesystem::create_directory(lock_dir, ec)) {
    _write_pid(lock_dir);
    return true;
  }

  pid_t pid = 0;
  if (!_read_pid_file(lock_dir / std::string(constants::runtime::pid_file_name), pid) ||
      !platform::is_process_running(pid))
  {
    platform::safe_delete_directory(lock_dir);
    if (std::filesystem::create_directory(lock_dir, ec)) {
      _write_pid(lock_dir);
      return true;
    }
  }

  return false;
}

void release_lock(const std::filesystem::path& runtime_dir)
{
  platform::safe_delete_directory(_lock_dir_path(runtime_dir));
}

bool has_live_recorder(const std::filesystem::path& runtime_dir, pid_t& pid)
{
  return _read_pid_file(recorder_pid_path(runtime_dir), pid) && platform::is_process_running(pid);
}

void clean_payload(const std::filesystem::path& runtime_dir)
{
  using PathBuilder = std::filesystem::path (*)(const std::filesystem::path&);

  static constexpr PathBuilder targets[] = {recorder_pid_path, recorder_wav_path,
                                            recorder_error_path, cancel_marker_path, _state_path};

  for (const auto& build_path : targets) {
    platform::safe_delete_file(build_path(runtime_dir));
  }
}

void write_state(const std::filesystem::path& runtime_dir, const std::string& state)
{
  std::ofstream file(_state_path(runtime_dir));
  file << state << "\n";
}

std::string status_for(const std::filesystem::path& runtime_dir)
{
  const auto state = _read_state(runtime_dir);

  pid_t rec_pid = 0;
  if (has_live_recorder(runtime_dir, rec_pid)) {
    return std::string(constants::runtime::recording_state);
  }

  if (state == constants::runtime::transcribing_state && _has_live_lock(runtime_dir)) {
    return std::string(constants::runtime::transcribing_state);
  }

  return std::string(constants::runtime::idle_state);
}

bool wait_for_idle(const std::filesystem::path& runtime_dir)
{
  for (int attempt = 0; attempt < 100; ++attempt) {
    if (status_for(runtime_dir) == constants::runtime::idle_state) {
      return true;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  return status_for(runtime_dir) == constants::runtime::idle_state;
}

bool create_cancel_marker(const std::filesystem::path& runtime_dir)
{
  std::filesystem::create_directories(runtime_dir);
  const auto marker_path = cancel_marker_path(runtime_dir);
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

bool cancel_requested(const std::filesystem::path& runtime_dir)
{
  return std::filesystem::exists(cancel_marker_path(runtime_dir));
}

std::string trim(std::string value)
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

std::string recorder_error_text(const std::filesystem::path& runtime_dir)
{
  return trim(_read_text_file(recorder_error_path(runtime_dir)));
}

void print_recorder_error(const std::filesystem::path& runtime_dir)
{
  const auto error = recorder_error_text(runtime_dir);
  if (!error.empty()) {
    std::cerr << error << "\n";
  }
}

std::filesystem::path write_log(const std::filesystem::path& runtime_dir,
                                const std::string& content)
{
  std::filesystem::create_directories(runtime_dir);
  const auto path = runtime_dir / std::string(constants::runtime::error_log_file);
  std::ofstream file(path);
  file << content;
  return path;
}

} // namespace runtime::session
