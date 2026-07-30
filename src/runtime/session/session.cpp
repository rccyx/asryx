#include "runtime/session/session.hpp"

#include "constants/constants.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"

#include <array>
#include <cctype>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <utility>

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

std::optional<pid_t> _read_pid_file(const std::filesystem::path& path)
{
  std::ifstream file(path);
  pid_t pid = 0;
  if (file >> pid) {
    return pid;
  }

  return std::nullopt;
}

yx::Result<void> _create_directory(const std::filesystem::path& path)
{
  std::error_code error;
  std::filesystem::create_directories(path, error);
  if (error) {
    return yx::fail("failed to create runtime directory: " + error.message());
  }

  return yx::ok();
}

yx::Result<void> _write_text_file(const std::filesystem::path& path, const std::string& content)
{
  std::ofstream file(path);
  if (!file.is_open()) {
    return yx::fail("failed to open file for writing: " + path.string());
  }
  file << content;
  if (!file) {
    return yx::fail("failed to write file: " + path.string());
  }
  file.flush();
  if (!file) {
    return yx::fail("failed to flush file: " + path.string());
  }
  file.close();
  if (!file) {
    return yx::fail("failed to close file: " + path.string());
  }
  return yx::ok();
}

yx::Result<void> _write_pid(const std::filesystem::path& lock_dir)
{
  return _write_text_file(lock_dir / std::string(constants::runtime::pid_file_name),
                          std::to_string(getpid()) + "\n");
}

bool _has_live_lock(const std::filesystem::path& runtime_dir)
{
  const auto pid_file =
      _lock_dir_path(runtime_dir) / std::string(constants::runtime::pid_file_name);
  const auto maybe_pid = _read_pid_file(pid_file);
  return maybe_pid.has_value() && platform::is_process_running(*maybe_pid);
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

yx::Result<bool> acquire_lock(const std::filesystem::path& runtime_dir)
{
  const auto runtime_created = _create_directory(runtime_dir);
  if (!runtime_created) {
    return yx::fail(runtime_created.error());
  }
  const auto lock_dir = _lock_dir_path(runtime_dir);

  std::error_code ec;
  if (std::filesystem::create_directory(lock_dir, ec)) {
    return _write_pid(lock_dir).transform([] { return true; });
  }

  if (ec && ec != std::errc::file_exists) {
    return yx::fail("failed to create lock directory: " + ec.message());
  }

  const auto maybe_pid = _read_pid_file(lock_dir / std::string(constants::runtime::pid_file_name));
  if (!maybe_pid.has_value() || !platform::is_process_running(*maybe_pid)) {
    const auto deleted = platform::safe_delete_directory(lock_dir);
    if (!deleted) {
      return yx::fail(deleted.error());
    }

    ec.clear();
    if (std::filesystem::create_directory(lock_dir, ec)) {
      return _write_pid(lock_dir).transform([] { return true; });
    }

    if (ec && ec != std::errc::file_exists) {
      return yx::fail("failed to create lock directory: " + ec.message());
    }
  }

  return yx::ok(false);
}

yx::Result<void> release_lock(const std::filesystem::path& runtime_dir)
{
  return platform::safe_delete_directory(_lock_dir_path(runtime_dir));
}

std::optional<pid_t> live_recorder_pid(const std::filesystem::path& runtime_dir)
{
  const auto pid = _read_pid_file(recorder_pid_path(runtime_dir));
  if (pid.has_value() && platform::is_process_running(*pid)) {
    return pid;
  }

  return std::nullopt;
}

yx::Result<void> write_recorder_pid(const std::filesystem::path& runtime_dir, pid_t pid)
{
  const auto runtime_created = _create_directory(runtime_dir);
  if (!runtime_created) {
    return yx::fail(runtime_created.error());
  }
  return _write_text_file(recorder_pid_path(runtime_dir), std::to_string(pid) + "\n");
}

yx::Result<void> clean_payload(const std::filesystem::path& runtime_dir)
{
  using PathBuilder = std::filesystem::path (*)(const std::filesystem::path&);

  static constexpr std::array<PathBuilder, 5> targets = {
      recorder_pid_path, recorder_wav_path, recorder_error_path, cancel_marker_path, _state_path};

  for (const auto& build_path : targets) {
    const auto deleted = platform::safe_delete_file(build_path(runtime_dir));
    if (!deleted) {
      return yx::fail(deleted.error());
    }
  }

  return yx::ok();
}

yx::Result<void> write_state(const std::filesystem::path& runtime_dir, const std::string& state)
{
  const auto runtime_created = _create_directory(runtime_dir);
  if (!runtime_created) {
    return yx::fail(runtime_created.error());
  }

  return _write_text_file(_state_path(runtime_dir), state + "\n");
}

std::string status_for(const std::filesystem::path& runtime_dir)
{
  const auto state = _read_state(runtime_dir);

  if (live_recorder_pid(runtime_dir).has_value()) {
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

yx::Result<bool> create_cancel_marker(const std::filesystem::path& runtime_dir)
{
  const auto runtime_created = _create_directory(runtime_dir);
  if (!runtime_created) {
    return yx::fail(runtime_created.error());
  }

  const auto marker_path = cancel_marker_path(runtime_dir);
  const int fd = open(marker_path.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
  if (fd == -1) {
    if (errno == EEXIST) {
      return yx::ok(false);
    }

    return yx::fail("failed to create cancel marker: " + marker_path.string());
  }

  const int close_status = close(fd);
  if (close_status == -1) {
    const auto deleted = platform::safe_delete_file(marker_path);
    if (!deleted) {
      return yx::fail(deleted.error());
    }

    return yx::fail("failed to close cancel marker: " + std::string(std::strerror(errno)));
  }

  return yx::ok(true);
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

yx::Result<std::filesystem::path> write_log(const std::filesystem::path& runtime_dir,
                                            const std::string& content)
{
  const auto runtime_created = _create_directory(runtime_dir);
  if (!runtime_created) {
    return yx::fail(runtime_created.error());
  }

  auto path = runtime_dir / std::string(constants::runtime::error_log_file);
  const auto written = _write_text_file(path, content);
  if (!written) {
    return yx::fail(written.error());
  }

  return yx::ok(std::move(path));
}

} // namespace runtime::session
