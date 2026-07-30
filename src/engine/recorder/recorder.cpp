#include "engine/recorder/recorder.hpp"

#include "platform/process.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <vector>

namespace engine::recorder {

namespace {

bool _wait_until_exited(pid_t pid)
{
  for (int attempt = 0; attempt < 100; ++attempt) {
    int status = 0;
    const pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      return true;
    }

    if (result == -1) {
      if (errno != ECHILD) {
        return false;
      }

      if (!platform::is_process_running(pid)) {
        return true;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  return false;
}

yx::Result<std::vector<std::string>> _args(const std::string& wav_path)
{
  const auto has_pw_record = platform::command_exists("pw-record");
  if (!has_pw_record) {
    return yx::fail(has_pw_record.error());
  }

  if (*has_pw_record) {
    return yx::ok(std::vector<std::string>{"pw-record", "--format=s16", "--rate=16000",
                                           "--channels=1", wav_path});
  }

  const auto has_arecord = platform::command_exists("arecord");
  if (!has_arecord) {
    return yx::fail(has_arecord.error());
  }

  if (*has_arecord) {
    return yx::ok(std::vector<std::string>{"arecord", "-q", "-t", "wav", "-f", "S16_LE", "-c", "1",
                                           "-r", "16000", wav_path});
  }

  return yx::fail("No recorder tool found (need pw-record or arecord)");
}

} // namespace

yx::Result<pid_t> start(const std::string& wav_path, const std::string& err_path)
{
  const auto args = _args(wav_path);
  if (!args) {
    return yx::fail(args.error());
  }

  const auto pid = platform::spawn_process_background(*args, err_path);
  if (!pid) {
    return yx::fail(pid.error());
  }

  return yx::ok(*pid);
}

yx::Result<bool> stop(pid_t pid)
{
  if (pid <= 0) {
    return yx::fail("invalid recorder process id");
  }

  const auto interrupted = platform::stop_process(pid, SIGINT);
  if (!interrupted) {
    return yx::fail(interrupted.error());
  }

  if (_wait_until_exited(pid)) {
    return yx::ok(true);
  }

  const auto terminated = platform::stop_process(pid, SIGTERM);
  if (!terminated) {
    return yx::fail(terminated.error());
  }

  if (_wait_until_exited(pid)) {
    return yx::ok(true);
  }

  const auto killed = platform::stop_process(pid, SIGKILL);
  if (!killed) {
    return yx::fail(killed.error());
  }

  return yx::ok(_wait_until_exited(pid));
}

} // namespace engine::recorder
