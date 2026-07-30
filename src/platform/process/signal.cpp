#include "platform/process.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <unistd.h>

namespace platform {

bool is_process_running(pid_t pid)
{
  if (pid <= 0) {
    return false;
  }

  return kill(pid, 0) == 0;
}

yx::Result<bool> stop_process(pid_t pid, int sig)
{
  if (pid <= 0) {
    return yx::fail("invalid process id");
  }

  if (kill(pid, sig) == 0) {
    return yx::ok(true);
  }

  if (errno == ESRCH) {
    return yx::ok(true);
  }

  return yx::fail("failed to signal process: " + std::string(std::strerror(errno)));
}

} // namespace platform
