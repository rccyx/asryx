#include "platform/process.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iterator>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>

namespace platform {

namespace {

std::vector<char*> _build_argv(const std::vector<std::string>& argv)
{
  std::vector<char*> c_argv;
  c_argv.reserve(argv.size() + 1);

  std::transform(argv.begin(), argv.end(), std::back_inserter(c_argv), [](const std::string& arg) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    return const_cast<char*>(arg.c_str());
  });

  c_argv.push_back(nullptr);
  return c_argv;
}

void _redirect_stdout_to_devnull()
{
  const int dev_null = open("/dev/null", O_WRONLY);
  if (dev_null != -1) {
    dup2(dev_null, STDOUT_FILENO);
    close(dev_null);
  }
}

void _redirect_stderr_to_file(const std::string& path)
{
  if (path.empty()) {
    return;
  }

  const int file_descriptor = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (file_descriptor != -1) {
    dup2(file_descriptor, STDERR_FILENO);
    close(file_descriptor);
  }
}

} // namespace

std::expected<bool, asryx::Error> command_exists(const std::string& name)
{
  static std::unordered_map<std::string, bool> cache;

  const auto cached = cache.find(name);
  if (cached != cache.end()) {
    return cached->second;
  }

  const pid_t pid = fork();
  if (pid == 0) {
    const int dev_null = open("/dev/null", O_WRONLY);
    if (dev_null != -1) {
      dup2(dev_null, STDOUT_FILENO);
      dup2(dev_null, STDERR_FILENO);
      close(dev_null);
    }

    execlp("which", "which", name.c_str(), nullptr);
    _exit(127);
  }

  if (pid < 0) {
    return asryx::fail("failed to check command: " + name);
  }

  int status = 0;
  waitpid(pid, &status, 0);

  const bool found = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  cache[name] = found;
  return found;
}

std::expected<pid_t, asryx::Error> spawn_process_background(
    const std::vector<std::string>& argv,
    const std::string& redirect_file)
{
  if (argv.empty()) {
    return asryx::fail("cannot spawn empty command");
  }

  const pid_t pid = fork();
  if (pid < 0) {
    return asryx::fail("failed to fork process");
  }

  if (pid == 0) {
    _redirect_stdout_to_devnull();
    _redirect_stderr_to_file(redirect_file);

    auto process_args = _build_argv(argv);
    execvp(process_args[0], process_args.data());
    _exit(127);
  }

  return pid;
}

std::expected<int, asryx::Error> wait_process(pid_t pid)
{
  int status = 0;
  if (waitpid(pid, &status, 0) == -1) {
    return asryx::fail("failed to wait for process: " + std::string(std::strerror(errno)));
  }

  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

std::expected<bool, asryx::Error> run_process_blocking(const std::vector<std::string>& argv)
{
  if (argv.empty()) {
    return asryx::fail("cannot run empty command");
  }

  const pid_t pid = fork();
  if (pid < 0) {
    return asryx::fail("failed to fork process");
  }

  if (pid == 0) {
    auto process_args = _build_argv(argv);
    execvp(process_args[0], process_args.data());
    _exit(127);
  }

  return wait_process(pid).transform([](int status) { return status == 0; });
}

std::expected<bool, asryx::Error> run_process_with_stdin(const std::vector<std::string>& argv,
                                                         const std::string& input)
{
  if (argv.empty()) {
    return asryx::fail("cannot run empty command");
  }

  std::array<int, 2> pipe_fds{};
  if (pipe(pipe_fds.data()) != 0) {
    return asryx::fail("failed to create stdin pipe");
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return asryx::fail("failed to fork process");
  }

  if (pid == 0) {
    (void)signal(SIGPIPE, SIG_DFL);
    close(pipe_fds[1]);
    dup2(pipe_fds[0], STDIN_FILENO);
    close(pipe_fds[0]);

    auto process_args = _build_argv(argv);
    execvp(process_args[0], process_args.data());
    _exit(127);
  }

  close(pipe_fds[0]);

  auto previous_sigpipe = signal(SIGPIPE, SIG_IGN);
  const char* ptr = input.data();
  size_t remaining = input.size();

  while (remaining > 0) {
    ssize_t written = write(pipe_fds[1], ptr, remaining);
    if (written <= 0) {
      break;
    }

    ptr += static_cast<size_t>(written);
    remaining -= static_cast<size_t>(written);
  }

  close(pipe_fds[1]);
  (void)signal(SIGPIPE, previous_sigpipe);
  return wait_process(pid).transform([](int status) { return status == 0; });
}

bool is_process_running(pid_t pid)
{
  if (pid <= 0) {
    return false;
  }

  return kill(pid, 0) == 0;
}

std::expected<bool, asryx::Error> stop_process(pid_t pid, int sig)
{
  if (pid <= 0) {
    return asryx::fail("invalid process id");
  }

  return kill(pid, sig) == 0;
}

} // namespace platform
