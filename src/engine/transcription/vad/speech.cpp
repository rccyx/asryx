#include "engine/transcription/vad/speech.hpp"

#include <cctype>
#include <cstddef>

namespace engine::transcription::vad {

namespace {

bool _is_utf8_continuation(unsigned char byte)
{
  return (byte & 0xC0U) == 0x80U;
}

std::size_t _content_unit_count(const std::string& text)
{
  std::size_t count = 0;
  bool in_ascii_word = false;

  for (char character : text) {
    const auto byte = static_cast<unsigned char>(character);
    if (std::isalnum(byte) != 0) {
      if (!in_ascii_word) {
        ++count;
        in_ascii_word = true;
      }

      continue;
    }

    in_ascii_word = false;
    if (byte >= 0x80U && !_is_utf8_continuation(byte)) {
      ++count;
    }
  }

  return count;
}

std::size_t _minimum_content_units(double vad_speech_s)
{
  if (vad_speech_s < 1.0) {
    return 1;
  }

  const auto expected_units = static_cast<std::size_t>(vad_speech_s / 8.0);
  return expected_units == 0 ? 1 : expected_units;
}

} // namespace

bool looks_suspicious(const std::string& text, double vad_speech_s)
{
  if (vad_speech_s < 1.0) {
    return false;
  }

  const std::size_t content_units = _content_unit_count(text);
  if (content_units == 0) {
    return true;
  }

  return content_units < _minimum_content_units(vad_speech_s);
}

bool retry_is_better(const std::string& primary, const std::string& retry, double vad_speech_s)
{
  const std::size_t retry_units = _content_unit_count(retry);
  if (retry_units == 0) {
    return false;
  }

  if (!looks_suspicious(retry, vad_speech_s)) {
    return true;
  }

  const std::size_t primary_units = _content_unit_count(primary);
  if (primary_units == 0) {
    return true;
  }

  return retry_units >= primary_units * 2U;
}

} // namespace engine::transcription::vad
