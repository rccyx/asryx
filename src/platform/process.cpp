#include "platform/process.hpp"

#include <algorithm>
#include <array>
#include <csignal>
#include <fcntl.h>
#include <iterator>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <unordered_map>

namespace platform {

bool command_exists(const std::string& name)
{
  static std::unordered_map<std::string, bool> cache;

  auto cached = cache.find(name);
  if (cached != cache.end()) {
    return cached->second;
  }

  pid_t pid = fork();
  if (pid == 0) {
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDOUT_FILENO);
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    execlp("which", "which", name.c_str(), nullptr);
    _exit(127);
  }

  if (pid < 0) {
    cache[name] = false;
    return false;
  }

  int status = 0;
  waitpid(pid, &status, 0);

  const bool found = WIFEXITED(status) && WEXITSTATUS(status) == 0;
  cache[name] = found;
  return found;
}

namespace {

std::vector<char*> build_argv(const std::vector<std::string>& argv)
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

void redirect_stdout_to_devnull()
{
  int fd = open("/dev/null", O_WRONLY);
  if (fd != -1) {
    dup2(fd, STDOUT_FILENO);
    close(fd);
  }
}

void redirect_stderr_to_file(const std::string& path)
{
  if (path.empty()) {
    return;
  }

  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
  if (fd != -1) {
    dup2(fd, STDERR_FILENO);
    close(fd);
  }
}

} // namespace

pid_t spawn_process_background(const std::vector<std::string>& argv,
                               const std::string& redirect_file)
{
  if (argv.empty()) {
    return -1;
  }

  pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }

  if (pid == 0) {
    redirect_stdout_to_devnull();
    redirect_stderr_to_file(redirect_file);

    auto c_argv = build_argv(argv);
    execvp(c_argv[0], c_argv.data());
    _exit(127);
  }

  return pid;
}

int wait_process(pid_t pid)
{
  int status = 0;
  waitpid(pid, &status, 0);
  return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

bool run_process_blocking(const std::vector<std::string>& argv)
{
  if (argv.empty()) {
    return false;
  }

  pid_t pid = fork();
  if (pid < 0) {
    return false;
  }

  if (pid == 0) {
    auto c_argv = build_argv(argv);
    execvp(c_argv[0], c_argv.data());
    _exit(127);
  }

  return wait_process(pid) == 0;
}

bool run_process_with_stdin(const std::vector<std::string>& argv, const std::string& input)
{
  if (argv.empty()) {
    return false;
  }

  std::array<int, 2> pipe_fds{};
  if (pipe(pipe_fds.data()) != 0) {
    return false;
  }

  pid_t pid = fork();
  if (pid < 0) {
    close(pipe_fds[0]);
    close(pipe_fds[1]);
    return false;
  }

  if (pid == 0) {
    close(pipe_fds[1]);
    dup2(pipe_fds[0], STDIN_FILENO);
    close(pipe_fds[0]);

    auto c_argv = build_argv(argv);
    execvp(c_argv[0], c_argv.data());
    _exit(127);
  }

  close(pipe_fds[0]);

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
  return wait_process(pid) == 0;
}

bool is_process_running(pid_t pid)
{
  if (pid <= 0) {
    return false;
  }

  return kill(pid, 0) == 0;
}

bool stop_process(pid_t pid, int sig)
{
  if (pid <= 0) {
    return false;
  }

  return kill(pid, sig) == 0;
}

} // namespace platform