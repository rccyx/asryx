#include "platform/process/exec.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <iterator>
#include <unistd.h>

namespace platform::process_detail {

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
  const int dev_null = open("/dev/null", O_WRONLY);
  if (dev_null != -1) {
    dup2(dev_null, STDOUT_FILENO);
    close(dev_null);
  }
}

void redirect_stderr_to_file(const std::string& path)
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

yx::Result<void> close_fd(int fd)
{
  if (close(fd) == -1) {
    return yx::fail("failed to close file descriptor: " + std::string(std::strerror(errno)));
  }

  return yx::ok();
}

yx::Result<void> write_all(int fd, const std::string& input)
{
  const char* cursor = input.data();
  size_t remaining = input.size();

  while (remaining > 0) {
    const ssize_t written = write(fd, cursor, remaining);
    if (written == 0) {
      return yx::fail("failed to write process stdin: wrote zero bytes");
    }

    if (written == -1) {
      if (errno == EINTR) {
        continue;
      }

      return yx::fail("failed to write process stdin: " + std::string(std::strerror(errno)));
    }

    cursor += static_cast<size_t>(written);
    remaining -= static_cast<size_t>(written);
  }

  return yx::ok();
}

} // namespace platform::process_detail
