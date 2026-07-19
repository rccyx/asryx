#include "engine/recorder/recorder.hpp"

#include "platform/process.hpp"

#include <cerrno>
#include <chrono>
#include <csignal>
#include <stdexcept>
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

std::vector<std::string> _args(const std::string& wav_path)
{
  if (platform::command_exists("pw-record")) {
    return {"pw-record", "--format=s16", "--rate=16000", "--channels=1", wav_path};
  }

  if (platform::command_exists("arecord")) {
    return {"arecord", "-q", "-t", "wav", "-f", "S16_LE", "-c", "1", "-r", "16000", wav_path};
  }

  throw std::runtime_error("No recorder tool found (need pw-record or arecord)");
}

} // namespace

pid_t start(const std::string& wav_path, const std::string& err_path)
{
  const auto args = _args(wav_path);
  const pid_t pid = platform::spawn_process_background(args, err_path);
  if (pid == -1) {
    throw std::runtime_error("Failed to start recorder process");
  }

  return pid;
}

bool stop(pid_t pid)
{
  if (pid <= 0) {
    return false;
  }

  platform::stop_process(pid, SIGINT);
  if (_wait_until_exited(pid)) {
    return true;
  }

  platform::stop_process(pid, SIGTERM);
  if (_wait_until_exited(pid)) {
    return true;
  }

  platform::stop_process(pid, SIGKILL);
  return _wait_until_exited(pid);
}

} // namespace engine::recorder
