#include "platform/process.hpp"
#include "platform/process/exec.hpp"

#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

namespace platform {

yx::Result<pid_t> spawn_process_background(const std::vector<std::string>& argv,
                                           const std::string& redirect_file)
{
  if (argv.empty()) {
    return yx::fail("cannot spawn empty command");
  }

  const pid_t pid = fork();
  if (pid < 0) {
    return yx::fail("failed to fork process");
  }

  if (pid == 0) {
    process_detail::redirect_stdout_to_devnull();
    process_detail::redirect_stderr_to_file(redirect_file);

    auto process_args = process_detail::build_argv(argv);
    execvp(process_args[0], process_args.data());
    _exit(127);
  }

  return yx::ok(pid);
}

yx::Result<int> wait_process(pid_t pid)
{
  int status = 0;
  if (waitpid(pid, &status, 0) == -1) {
    return yx::fail("failed to wait for process: " + std::string(std::strerror(errno)));
  }

  return yx::ok(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
}

yx::Result<bool> run_process_blocking(const std::vector<std::string>& argv)
{
  if (argv.empty()) {
    return yx::fail("cannot run empty command");
  }

  const pid_t pid = fork();
  if (pid < 0) {
    return yx::fail("failed to fork process");
  }

  if (pid == 0) {
    auto process_args = process_detail::build_argv(argv);
    execvp(process_args[0], process_args.data());
    _exit(127);
  }

  return wait_process(pid).transform([](int status) { return status == 0; });
}

} // namespace platform
