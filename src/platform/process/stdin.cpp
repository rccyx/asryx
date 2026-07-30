#include "platform/process.hpp"
#include "platform/process/exec.hpp"

#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <unistd.h>

namespace platform {

namespace {

yx::Result<void> _ignore_sigpipe(struct sigaction& previous)
{
  struct sigaction ignored{};
  ignored.sa_handler = SIG_IGN;
  sigemptyset(&ignored.sa_mask);

  if (sigaction(SIGPIPE, &ignored, &previous) == -1) {
    return yx::fail("failed to ignore SIGPIPE: " + std::string(std::strerror(errno)));
  }

  return yx::ok();
}

yx::Result<void> _restore_sigpipe(const struct sigaction& previous)
{
  if (sigaction(SIGPIPE, &previous, nullptr) == -1) {
    return yx::fail("failed to restore SIGPIPE handler: " + std::string(std::strerror(errno)));
  }

  return yx::ok();
}

void _connect_stdin_and_exec(const std::vector<std::string>& argv, int read_fd, int write_fd)
{
  (void)signal(SIGPIPE, SIG_DFL);
  yx::ignore_failure(process_detail::close_fd(write_fd));
  dup2(read_fd, STDIN_FILENO);
  yx::ignore_failure(process_detail::close_fd(read_fd));

  auto process_args = process_detail::build_argv(argv);
  execvp(process_args[0], process_args.data());
  _exit(127);
}

} // namespace

yx::Result<bool> run_process_with_stdin(const std::vector<std::string>& argv,
                                        const std::string& input)
{
  if (argv.empty()) {
    return yx::fail("cannot run empty command");
  }

  std::array<int, 2> pipe_fds{};
  if (pipe(pipe_fds.data()) != 0) {
    return yx::fail("failed to create stdin pipe");
  }

  const int read_fd = pipe_fds[0];
  const int write_fd = pipe_fds[1];
  const pid_t pid = fork();
  if (pid < 0) {
    yx::ignore_failure(process_detail::close_fd(read_fd));
    yx::ignore_failure(process_detail::close_fd(write_fd));
    return yx::fail("failed to fork process");
  }

  if (pid == 0) {
    _connect_stdin_and_exec(argv, read_fd, write_fd);
  }

  const auto closed_read = process_detail::close_fd(read_fd);
  if (!closed_read) {
    yx::ignore_failure(process_detail::close_fd(write_fd));
    yx::ignore_failure(wait_process(pid));
    return yx::fail(closed_read.error());
  }

  struct sigaction previous_sigpipe{};
  const auto sigpipe_ignored = _ignore_sigpipe(previous_sigpipe);
  if (!sigpipe_ignored) {
    yx::ignore_failure(process_detail::close_fd(write_fd));
    yx::ignore_failure(wait_process(pid));
    return yx::fail(sigpipe_ignored.error());
  }

  const auto written = process_detail::write_all(write_fd, input);
  const auto closed_write = process_detail::close_fd(write_fd);
  const auto restored_sigpipe = _restore_sigpipe(previous_sigpipe);
  const auto waited = wait_process(pid);
  if (!written) {
    return yx::fail(written.error());
  }

  if (!closed_write) {
    return yx::fail(closed_write.error());
  }

  if (!restored_sigpipe) {
    return yx::fail(restored_sigpipe.error());
  }

  if (!waited) {
    return yx::fail(waited.error());
  }

  return waited.transform([](int status) { return status == 0; });
}

} // namespace platform
