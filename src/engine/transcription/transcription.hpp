#ifndef ASRYX_ENGINE_TRANSCRIPTION_HPP
#define ASRYX_ENGINE_TRANSCRIPTION_HPP

#include "engine/engine.hpp"

#include <expected>
#include <string>

namespace engine::transcription {

std::expected<std::string, asryx::Error> run(const TranscriptionRequest& request);

} // namespace engine::transcription

#endif // ASRYX_ENGINE_TRANSCRIPTION_HPP
