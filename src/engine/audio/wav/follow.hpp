#ifndef ASRYX_ENGINE_AUDIO_WAV_FOLLOW_HPP
#define ASRYX_ENGINE_AUDIO_WAV_FOLLOW_HPP

#include "error.hpp"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace engine::audio {

class WavFollower final
{
public:
  static yx::Result<WavFollower> open(const std::filesystem::path& path);

private:
  int _fd;
  std::int64_t _offset;

public:
  explicit WavFollower(int fd, std::int64_t offset);
  WavFollower(WavFollower&& other) noexcept;
  WavFollower(const WavFollower&) = delete;
  WavFollower& operator=(const WavFollower&) = delete;
  WavFollower& operator=(WavFollower&&) = delete;
  ~WavFollower();
  yx::Result<std::vector<float>> read();
};

} // namespace engine::audio
#endif
