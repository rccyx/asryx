#ifndef ASRYX_ENGINE_TRANSCRIPTION_REQUEST_HPP
#define ASRYX_ENGINE_TRANSCRIPTION_REQUEST_HPP

#include "engine/engine.hpp"

#include <expected>

namespace engine::transcription::request {

std::expected<void, asryx::Error> validate(const TranscriptionRequest& request);

} // namespace engine::transcription::request

#endif // ASRYX_ENGINE_TRANSCRIPTION_REQUEST_HPP
