#include "engine/transcription/vad/speech.hpp"

#include <algorithm>
#include <cstddef>

namespace engine::transcription::vad {

bool looks_suspicious(const std::string& text, double vad_speech_s)
{
  if (vad_speech_s < 1.0) {
    return false;
  }

  if (text.empty()) {
    return true;
  }

  const auto word_count = static_cast<std::size_t>(std::count(text.begin(), text.end(), ' ')) + 1U;

  return word_count < static_cast<std::size_t>(vad_speech_s / 8.0);
}

} // namespace engine::transcription::vad
