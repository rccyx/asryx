#include "engine/audio/file/file.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace engine::audio {

namespace {

yx::Result<void> _close_file(int fd, const std::string& path)
{
  if (close(fd) == -1) {
    return yx::fail("failed to close wav file: " + path + ": " + std::strerror(errno));
  }

  return yx::ok();
}

yx::Result<void> _read_full(int fd, std::vector<std::uint8_t>& bytes, const std::string& path)
{
  std::uint8_t* cursor = bytes.data();
  size_t remaining = bytes.size();

  while (remaining > 0) {
    const ssize_t count = read(fd, cursor, remaining);
    if (count == -1) {
      if (errno == EINTR) {
        continue;
      }

      return yx::fail("failed to read wav file: " + path + ": " + std::strerror(errno));
    }

    if (count == 0) {
      return yx::fail("failed to read complete wav file: " + path);
    }

    cursor += static_cast<size_t>(count);
    remaining -= static_cast<size_t>(count);
  }

  return yx::ok();
}

} // namespace

yx::Result<std::vector<std::uint8_t>> read_audio_file(const std::string& path)
{
  const int fd = open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd == -1) {
    return yx::fail("failed to open wav file: " + path + ": " + std::strerror(errno));
  }

  struct stat status{};
  if (fstat(fd, &status) == -1) {
    const std::string error = std::strerror(errno);
    const auto closed = _close_file(fd, path);
    if (!closed) {
      return yx::fail(closed.error());
    }

    return yx::fail("failed to determine wav file size: " + path + ": " + error);
  }

  if (status.st_size < 0) {
    yx::ignore_failure(_close_file(fd, path));
    return yx::fail("failed to determine wav file size: " + path);
  }

  const auto unsigned_size = static_cast<std::uintmax_t>(status.st_size);
  const auto max_stream_size = static_cast<std::uintmax_t>(std::numeric_limits<ssize_t>::max());
  if (unsigned_size > max_stream_size) {
    yx::ignore_failure(_close_file(fd, path));
    return yx::fail("wav file is too large to read: " + path);
  }

  const auto size = static_cast<size_t>(unsigned_size);
  if (size == 0) {
    yx::ignore_failure(_close_file(fd, path));
    return yx::fail("wav file is empty: " + path);
  }

  std::vector<std::uint8_t> bytes(size);
  const auto read = _read_full(fd, bytes, path);
  const auto closed = _close_file(fd, path);
  if (!read) {
    return yx::fail(read.error());
  }

  if (!closed) {
    return yx::fail(closed.error());
  }

  return yx::ok(std::move(bytes));
}

} // namespace engine::audio
