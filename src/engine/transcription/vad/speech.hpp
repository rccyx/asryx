#ifndef ASRYX_ENGINE_TRANSCRIPTION_VAD_SPEECH_HPP
#define ASRYX_ENGINE_TRANSCRIPTION_VAD_SPEECH_HPP

#include <cstddef>
#include <string>

namespace engine::transcription::vad {

bool looks_suspicious(const std::string& text, double vad_speech_s);
bool retry_is_better(const std::string& primary, const std::string& retry, double vad_speech_s);

} // namespace engine::transcription::vad

#endif // ASRYX_ENGINE_TRANSCRIPTION_VAD_SPEECH_HPP
