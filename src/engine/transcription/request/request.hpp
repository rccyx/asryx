#ifndef ASRYX_ENGINE_TRANSCRIPTION_REQUEST_HPP
#define ASRYX_ENGINE_TRANSCRIPTION_REQUEST_HPP

#include "engine/engine.hpp"

namespace engine::transcription::request {

yx::Result<void> validate(const TranscriptionRequest& request);

} // namespace engine::transcription::request

#endif // ASRYX_ENGINE_TRANSCRIPTION_REQUEST_HPP
