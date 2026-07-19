#include "engine/transcription/transcription.hpp"

#include "constants/constants.hpp"
#include "engine/audio/audio.hpp"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#endif

#include <whisper.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

namespace engine::transcription {

namespace {

struct WhisperContextDeleter
{
  void operator()(whisper_context* ctx) const
  {
    if (ctx != nullptr) {
      whisper_free(ctx);
    }
  }
};

using WhisperContext = std::unique_ptr<whisper_context, WhisperContextDeleter>;

void _validate_request(const TranscriptionRequest& request)
{
  if (!std::filesystem::exists(request.model_path)) {
    throw std::runtime_error("model file does not exist: " + request.model_path);
  }

  if (!std::filesystem::exists(request.vad_model_path)) {
    throw std::runtime_error("VAD model file does not exist: " + request.vad_model_path);
  }
}

int _thread_count()
{
  const auto detected = std::thread::hardware_concurrency();
  if (detected == 0) {
    return 4;
  }

  return static_cast<int>(std::min(4U, detected));
}

const char* _language(whisper_context* ctx, const std::string& language)
{
  if (whisper_is_multilingual(ctx) == 0) {
    return constants::config::english_language.data();
  }

  if (language.empty() || language == constants::config::auto_language) {
    return nullptr;
  }

  return language.c_str();
}

// cppcheck-suppress constParameterCallback
bool _abort_requested(void* user_data)
{
  if (user_data == nullptr) {
    return false;
  }

  const auto* marker_path = static_cast<const std::string*>(user_data);
  return !marker_path->empty() && std::filesystem::exists(*marker_path);
}

WhisperContext _load_context(const std::string& model_path)
{
  whisper_context_params context_params = whisper_context_default_params();
  WhisperContext ctx(whisper_init_from_file_with_params(model_path.c_str(), context_params));
  if (ctx == nullptr) {
    throw std::runtime_error("failed to initialize whisper model: " + model_path);
  }

  return ctx;
}

whisper_full_params _params(whisper_context* ctx, TranscriptionRequest& request)
{
  whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

  params.temperature = 0.0F;
  params.temperature_inc = 0.2F;
  params.greedy.best_of = 1;
  params.no_context = false;

  params.n_threads = _thread_count();
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.no_timestamps = true;
  params.suppress_blank = true;
  params.suppress_nst = true;
  params.language = _language(ctx, request.language);
  params.detect_language = false;
  params.vad = true;
  params.vad_model_path = request.vad_model_path.c_str();
  params.vad_params = whisper_vad_default_params();
  params.abort_callback = _abort_requested;
  params.abort_callback_user_data = &request.cancel_marker_path;

  return params;
}

std::string _read_output(whisper_context* ctx)
{
  std::string output;
  const int segments = whisper_full_n_segments(ctx);

  for (int i = 0; i < segments; ++i) {
    const char* const text = whisper_full_get_segment_text(ctx, i);
    if (text != nullptr) {
      output += text;
    }
  }

  return output;
}

} // namespace

std::string run(const TranscriptionRequest& request)
{
  _validate_request(request);

  const auto samples = audio::read_pcm16_wav(request.wav_path);
  const auto ctx = _load_context(request.model_path);
  auto transcription_request = request;
  const auto params = _params(ctx.get(), transcription_request);

  if (whisper_full(ctx.get(), params, samples.data(), static_cast<int>(samples.size())) != 0) {
    if (!request.cancel_marker_path.empty() && std::filesystem::exists(request.cancel_marker_path))
    {
      throw TranscriptionCancelled();
    }

    throw std::runtime_error("transcription failed");
  }

  return _read_output(ctx.get());
}

} // namespace engine::transcription
