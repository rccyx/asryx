#ifndef ASRYX_ENGINE_TRANSCRIPTION_ROLLING_HPP
#define ASRYX_ENGINE_TRANSCRIPTION_ROLLING_HPP

#include "error.hpp"

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine::transcription::rolling {

inline constexpr std::uint64_t RATE = 16'000;

struct Word
{
  std::uint64_t begin;
  std::uint64_t end;
  std::string text;
};

using Decode =
    std::function<yx::Result<std::vector<Word>>(std::span<const float>, std::string_view)>;

class Processor final
{
private:
  Decode _decode;
  std::string _base_prompt;
  std::vector<float> _audio;
  std::vector<Word> _previous;
  std::string _committed;
  std::uint64_t _base = 0;
  std::uint64_t _received = 0;
  std::uint64_t _decoded_at = 0;

public:
  explicit Processor(Decode decode, std::string base_prompt = {});
  yx::Result<void> append(std::span<const float> samples);
  yx::Result<std::string> finish();

private:
  [[nodiscard]] std::string _build_prompt() const;
  yx::Result<void> _process(bool final);
  void _confirm(const std::vector<Word>& current);
  void _force_advance(const std::vector<Word>& current);
};

} // namespace engine::transcription::rolling

#endif
