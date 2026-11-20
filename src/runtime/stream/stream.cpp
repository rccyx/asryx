#include "runtime/stream/stream.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"
#include "runtime/session/session.hpp"
#include "runtime/stream/worker.hpp"
#include "runtime/transcription/transcription.hpp"

#include <chrono>
#include <csignal>
#include <fstream>
#include <iterator>
#include <optional>
#include <thread>

namespace runtime::stream {

namespace {
using Path = std::filesystem::path;

std::optional<pid_t> _worker_pid(const Path& dir)
{
  std::ifstream in(dir / "stream.pid");
  pid_t pid = 0;
  return (in >> pid) && pid > 0 ? std::optional(pid) : std::nullopt;
}

bool _running(pid_t pid)
{
  if (!platform::is_process_running(pid)) {
    return false;
  }
  // Orphaned children can remain zombies until their parent reaps them.
  std::ifstream proc("/proc/" + std::to_string(pid) + "/stat");
  std::string line;
  if (!std::getline(proc, line)) {
    return false;
  }
  const auto close = line.rfind(')');
  return close == std::string::npos || close + 2 >= line.size() ||
         (line[close + 2] != 'Z' && line[close + 2] != 'X');
}

yx::Result<void> _write_text(const Path& file, const std::string& text)
{
  std::ofstream out(file, std::ios::binary | std::ios::trunc);
  if (!out || !(out << text) || !out.flush()) {
    return yx::fail(yx::ErrorCode::filesystem, "cannot write rolling session file: " + file.string());
  }
  out.close();
  return out ? yx::ok() : yx::Result<void>(yx::fail(yx::ErrorCode::filesystem, "cannot close rolling session file"));
}

yx::Result<void> _await_worker(const Path& dir)
{
  const auto pid = _worker_pid(dir);
  while (!std::filesystem::exists(dir / "stream.done") &&
         !std::filesystem::exists(dir / "stream.failed"))
  {
    if (session::cancel_requested(dir) || !pid || !_running(*pid)) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
  }
  // Stop before fallback or cleanup can remove files still used by the worker.
  return cancel_worker(dir);
}

yx::Result<void> _deliver(const Path& dir)
{
  if (session::cancel_requested(dir)) {
    return session::clean_payload(dir);
  }
  if (!std::filesystem::exists(dir / "stream.done")) {
    return transcription::transcribe_stopped(dir);
  }
  std::ifstream in(dir / "stream.txt", std::ios::binary);
  if (!in) {
    return transcription::transcribe_stopped(dir);
  }
  const std::string output((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (in.bad()) {
    return transcription::transcribe_stopped(dir);
  }
  const auto trimmed = session::trim(output);
  if (trimmed.empty()) {
    yx::ignore_failure(engine::send_notification("no output"));
    return session::clean_payload(dir);
  }
  const auto cfg = config::load_config();
  if (!cfg) {
    return std::unexpected(cfg.error());
  }
  if (session::cancel_requested(dir)) {
    return session::clean_payload(dir);
  }
  return transcription::deliver({.runtime_dir = dir, .config = *cfg, .output = trimmed});
}
} // namespace

bool active(const Path& dir)
{
  return std::filesystem::exists(dir / "stream.pid");
}

yx::Result<void> start(const Path& dir)
{
  const auto child = platform::spawn_process_background({"/proc/self/exe", "--rolling-worker"},
                                                        (dir / "stream.err").string());
  if (!child) {
    return std::unexpected(child.error());
  }
  const auto saved = _write_text(dir / "stream.pid", std::to_string(*child) + "\n");
  if (!saved) {
    yx::ignore_failure(platform::stop_process(*child, SIGTERM));
  }
  return saved;
}

yx::Result<void> cancel_worker(const Path& dir)
{
  const auto pid = _worker_pid(dir);
  if (!pid || !_running(*pid)) {
    return yx::ok();
  }
  const auto stopped = platform::stop_process(*pid, SIGTERM);
  if (!stopped) {
    return std::unexpected(stopped.error());
  }
  for (int i = 0; i < 50 && _running(*pid); ++i) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (_running(*pid)) {
    const auto killed = platform::stop_process(*pid, SIGKILL);
    if (!killed) {
      return std::unexpected(killed.error());
    }
    while (_running(*pid)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }
  return yx::ok();
}

yx::Result<void> clean(const Path& dir)
{
  const auto stopped = cancel_worker(dir);
  if (!stopped) {
    return stopped;
  }
  for (const auto* name : {"stream.pid", "stream.finish", "stream.txt", "stream.done",
                           "stream.failed", "stream.err", "stream.txt.tmp"})
  {
    const auto deleted = platform::safe_delete_file(dir / name);
    if (!deleted) {
      return deleted;
    }
  }
  return yx::ok();
}

yx::Result<void> stop_and_deliver(const Path& dir, pid_t recorder)
{
  const auto stopped = engine::stop_recording(recorder);
  if (!stopped) {
    return std::unexpected(stopped.error());
  }
  if (!*stopped) {
    return yx::fail(yx::ErrorCode::recorder, "rolling recorder did not stop");
  }
  return session::write_state(dir, std::string(constants::runtime::transcribing_state))
      .and_then([&] { return _write_text(dir / "stream.finish", ""); })
      .and_then([&] { return _await_worker(dir); })
      .and_then([&] { return _deliver(dir); });
}

yx::Result<int> run_worker()
{
  const auto dir = platform::get_runtime_directory();
  const auto transcript = worker::compute(dir);
  if (session::cancel_requested(dir)) {
    return yx::ok(0);
  }
  if (!transcript) {
    return _write_text(dir / "stream.failed", transcript.error().message + "\n").transform([] {
      return 1;
    });
  }
  const auto written = _write_text(dir / "stream.txt.tmp", *transcript);
  if (!written) {
    return std::unexpected(written.error());
  }
  std::error_code error;
  std::filesystem::rename(dir / "stream.txt.tmp", dir / "stream.txt", error);
  if (error) {
    return yx::fail(yx::ErrorCode::filesystem, "cannot publish rolling transcript: " + error.message());
  }
  return _write_text(dir / "stream.done", "").transform([] { return 0; });
}

} // namespace runtime::stream
