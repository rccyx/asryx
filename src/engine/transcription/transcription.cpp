#include "engine/transcription/transcription.hpp"

#include "engine/audio/audio.hpp"
#include "engine/transcription/request/request.hpp"
#include "engine/transcription/vad/speech.hpp"
#include "engine/transcription/whisper/api.hpp"

#include <filesystem>
#include <span>
#include <string>

namespace engine::transcription {

std::expected<std::string, asryx::Error> run(const TranscriptionRequest& request)
{
  const auto valid_request = request::validate(request);
  if (!valid_request) {
    return std::unexpected(valid_request.error());
  }

  const auto samples = audio::read_pcm16_wav(request.wav_path);
  if (!samples) {
    return std::unexpected(samples.error());
  }

  auto ctx = whisper::load_context(request.model_path);
  if (!ctx) {
    return std::unexpected(ctx.error());
  }

  auto transcription_request = request;

  const auto primary_input = whisper::TranscribeInput{
      .ctx = &*ctx,
      .request = &transcription_request,
      .samples = std::span<const float>(*samples),
      .use_vad = true,
      .permissive_no_speech = false,
  };

  if (!whisper::transcribe(primary_input)) {
    if (!request.cancel_marker_path.empty() && std::filesystem::exists(request.cancel_marker_path))
    {
      return asryx::fail("transcription canceled");
    }

    return asryx::fail("transcription failed");
  }

  auto text = whisper::read_output(*ctx);
  const double vad_speech_s = whisper::detected_speech_seconds({
      .vad_model_path = request.vad_model_path,
      .samples = std::span<const float>(*samples),
  });

  // sometimes you speak for 30 seconds and then all you get is: "hey, hey" -> that's broken
  // a human speaking normally covers about 1 word every 8 seconds or so
  // and that's the absolute bare minimum.
  // So if the VAD detects 16s of actual speech and the word count is less than 2,
  // yeah, it's kind of sus. If Whisper only spits out like 4 words for a 42s monologue, it's
  // broken. so run it again without the VAD off this time and get the text, if it's still sus, well
  // we tried -> send the old one
  if (vad::looks_suspicious(text, vad_speech_s)) {
    const auto fallback_input = whisper::TranscribeInput{
        .ctx = &*ctx,
        .request = &transcription_request,
        .samples = std::span<const float>(*samples),
        .use_vad = false,
        .permissive_no_speech = true,
    };

    if (whisper::transcribe(fallback_input)) {
      text = whisper::read_output(*ctx);
    }
  }

  return text;
}

} // namespace engine::transcription
