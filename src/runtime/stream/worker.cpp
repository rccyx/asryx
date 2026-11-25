#include "runtime/stream/worker.hpp"

#include "config/config.hpp"
#include "engine/audio/wav/follow.hpp"
#include "engine/engine.hpp"
#include "engine/transcription/rolling/rolling.hpp"
#include "engine/transcription/whisper/api.hpp"
#include "model/model.hpp"
#include "runtime/session/session.hpp"

#include <algorithm>
#include <chrono>
#include <iterator>
#include <optional>
#include <thread>
#include <utility>

namespace runtime::stream::worker {

namespace {

namespace asr = engine::transcription;
using Path = std::filesystem::path;

yx::Result<engine::TranscriptionRequest> _request_for(const Path& dir)
{
  const auto cfg = config::load_config();
  if (!cfg) {
    return std::unexpected(cfg.error());
  }
  const auto language = model::transcription_language_for(*cfg);
  if (!language) {
    return std::unexpected(language.error());
  }
  const auto model_path = model::get_model_path(cfg->model);
  if (!model_path) {
    return std::unexpected(model_path.error());
  }
  const auto vad = model::get_vad_model_path();
  if (!vad) {
    return std::unexpected(vad.error());
  }
  return yx::ok(engine::TranscriptionRequest{
      .model_path = *model_path,
      .vad_model_path = *vad,
      .wav_path = session::recorder_wav_path(dir).string(),
      .language = *language,
      .prompt = cfg->prompt,
      .cancel_marker_path = session::cancel_marker_path(dir).string(),
  });
}

yx::Result<std::vector<asr::rolling::Word>> _decode(asr::whisper::TranscribeInput input)
{
  const auto end = asr::whisper::speech_end(
      {.vad_model_path = input.request->vad_model_path, .samples = input.samples});
  if (!end) {
    return std::unexpected(end.error());
  }
  if (*end == 0) {
    return yx::ok<std::vector<asr::rolling::Word>>();
  }

  // whisper's internal vad compresses time, but rolling cuts need original sample offsets.
  input.samples = input.samples.first(*end);
  if (!asr::whisper::transcribe(input)) {
    return yx::fail(yx::ErrorCode::transcription, "rolling Whisper decode failed");
  }
  auto words = asr::whisper::read_words(*input.ctx);
  std::vector<asr::rolling::Word> decoded;
  decoded.reserve(words.size());
  std::transform(words.begin(), words.end(), std::back_inserter(decoded),
                 [](asr::whisper::Word& word) {
                   return asr::rolling::Word{.begin = static_cast<std::uint64_t>(word.t0_cs) * 160,
                                             .end = static_cast<std::uint64_t>(word.t1_cs) * 160,
                                             .text = std::move(word.text)};
                 });
  return yx::ok(std::move(decoded));
}

yx::Result<engine::audio::WavFollower> _open_wav(const Path& dir)
{
  while (!session::cancel_requested(dir)) {
    auto opened = engine::audio::WavFollower::open(session::recorder_wav_path(dir));
    if (opened) {
      return opened;
    }
    if (std::filesystem::exists(dir / "stream.finish") || !session::live_recorder_pid(dir)) {
      return std::unexpected(opened.error());
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
  }
  return yx::fail(yx::ErrorCode::cancelled, "rolling transcription cancelled");
}

struct FollowInput
{
  engine::audio::WavFollower wav;
  asr::rolling::Processor processor;
};

yx::Result<std::string> _follow(const Path& dir, FollowInput input)
{
  auto& [wav, processor] = input;
  while (!session::cancel_requested(dir)) {
    const bool finishing = std::filesystem::exists(dir / "stream.finish");
    const auto audio = wav.read();
    if (!audio) {
      return std::unexpected(audio.error());
    }
    if (!audio->empty()) {
      const auto appended = processor.append(*audio);
      if (!appended) {
        return std::unexpected(appended.error());
      }
      continue;
    }
    if (!session::live_recorder_pid(dir)) {
      if (finishing) {
        return processor.finish();
      }
      return yx::fail(yx::ErrorCode::recorder, "recorder exited before rolling session completed");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(finishing ? 10 : 40));
  }
  return yx::fail(yx::ErrorCode::cancelled, "rolling transcription cancelled");
}

} // namespace

yx::Result<std::string> compute(const Path& dir)
{
  auto request = _request_for(dir);
  if (!request) {
    return std::unexpected(request.error());
  }
  auto context = asr::whisper::load_context(request->model_path);
  if (!context) {
    return std::unexpected(context.error());
  }
  asr::rolling::Processor processor(
      [&](std::span<const float> audio, std::string_view prompt) {
        return _decode({.ctx = &*context,
                        .request = &*request,
                        .samples = audio,
                        .use_vad = false,
                        .permissive_no_speech = false,
                        .prompt_override = std::string(prompt),
                        .fast_decode = true,
                        .token_timestamps = true});
      },
      request->prompt);
  auto wav = _open_wav(dir);
  if (!wav) {
    return std::unexpected(wav.error());
  }
  return _follow(dir, {.wav = std::move(*wav), .processor = std::move(processor)});
}

} // namespace runtime::stream::worker
