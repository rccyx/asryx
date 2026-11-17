#include "engine/audio/wav/follow.hpp"

#include "engine/audio/pcm/pcm.hpp"
#include "engine/audio/wav/format/format.hpp"
#include "engine/audio/wav/riff/riff.hpp"
#include "engine/audio/wav/wav.hpp"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace engine::audio {

namespace {

yx::Result<std::int64_t> _data_offset(const std::vector<std::uint8_t>& head)
{
  if (head.size() < 44 || !chunk_is(head, {0, "RIFF"}) || !chunk_is(head, {8, "WAVE"})) {
    return yx::fail("growing WAV header is not available or invalid");
  }
  bool format_ok = false;
  for (std::size_t pos = 12; pos + 8 <= head.size();) {
    const auto bytes = *read_u32_le(head, pos + 4);
    if (chunk_is(head, {pos, "fmt "})) {
      const auto format = read_wav_format({.bytes = &head, .offset = pos + 8, .chunk_size = bytes});
      format_ok = format && validate_wav_format(*format).has_value();
    }
    if (chunk_is(head, {pos, "data"})) {
      return format_ok ? yx::ok(static_cast<std::int64_t>(pos + 8))
                       : yx::Result<std::int64_t>(yx::fail("expected mono 16 kHz PCM16 WAV"));
    }
    const auto next = pos + 8 + static_cast<std::size_t>(bytes) + (bytes & 1U);
    if (next <= pos || next > head.size()) {
      break;
    }
    pos = next;
  }
  return yx::fail("WAV data header is not available yet");
}

} // namespace

WavFollower::WavFollower(int fd, std::int64_t offset)
    : _fd(fd)
    , _offset(offset)
{
}
WavFollower::WavFollower(WavFollower&& other) noexcept
    : _fd(std::exchange(other._fd, -1))
    , _offset(other._offset)
{
}
WavFollower::~WavFollower()
{
  if (_fd >= 0) {
    ::close(_fd);
  }
}

yx::Result<WavFollower> WavFollower::open(const std::filesystem::path& path)
{
  const int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return yx::fail("cannot open growing WAV: " + std::string(std::strerror(errno)));
  }
  std::vector<std::uint8_t> head(4096);
  ssize_t count;
  do {
    count = ::pread(fd, head.data(), head.size(), 0);
  } while (count < 0 && errno == EINTR);
  if (count < 0) {
    ::close(fd);
    return yx::fail("cannot read growing WAV header");
  }
  head.resize(static_cast<std::size_t>(count));
  const auto offset = _data_offset(head);
  if (!offset) {
    ::close(fd);
    return yx::fail(offset.error());
  }
  return yx::ok(WavFollower(fd, *offset));
}

yx::Result<std::vector<float>> WavFollower::read()
{
  struct stat stat{};
  if (::fstat(_fd, &stat) != 0 || stat.st_size < _offset) {
    return yx::fail("cannot follow growing WAV size");
  }
  const auto available = static_cast<std::size_t>(stat.st_size - _offset);
  const auto count = std::min<std::size_t>(65536, available) & ~std::size_t{1};
  if (count == 0) {
    return yx::ok(std::vector<float>{});
  }
  std::vector<std::uint8_t> bytes(count);
  ssize_t n;
  do {
    n = ::pread(_fd, bytes.data(), count, _offset);
  } while (n < 0 && errno == EINTR);
  if (n < 0) {
    return yx::fail("cannot read growing WAV: " + std::string(std::strerror(errno)));
  }
  n -= n % 2;
  if (n == 0) {
    return yx::ok(std::vector<float>{});
  }
  bytes.resize(static_cast<std::size_t>(n));
  _offset += n;
  return decode_pcm16({.bytes = &bytes, .offset = 0, .size = bytes.size()});
}

} // namespace engine::audio
