#ifndef ASRYX_ENGINE_TRANSCRIPTION_ROLLING_HPP
#define ASRYX_ENGINE_TRANSCRIPTION_ROLLING_HPP

#include "error.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace engine::transcription::rolling {

inline constexpr std::uint64_t RATE = 16'000;

struct Segment
{
  std::uint64_t begin;
  std::uint64_t end;
  std::string text;
};

using Decode = std::function<yx::Result<std::vector<Segment>>(std::span<const float>)>;

class Processor final
{
private:
  Decode _decode;
  std::vector<float> _audio;
  std::vector<Segment> _previous;
  std::string _committed;
  std::uint64_t _base = 0;
  std::uint64_t _received = 0;
  std::uint64_t _decoded_at = 0;

public:
  explicit Processor(Decode decode);
  yx::Result<void> append(std::span<const float> samples);
  yx::Result<std::string> finish();

private:
  yx::Result<void> _process(bool final);
  void _confirm(const std::vector<Segment>& current);
};

} // namespace engine::transcription::rolling

#endif
