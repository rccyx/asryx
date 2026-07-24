#include "engine/transcription/transcription.hpp"

#include "constants/constants.hpp"
#include "engine/audio/audio.hpp"
#include "engine/transcription/compute.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
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

constexpr double COLLAPSE_MIN_AUDIO_SECONDS = 8.0;
constexpr size_t COLLAPSE_MAX_WORDS = 3;

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

std::expected<void, asryx::Error> _validate_request(const TranscriptionRequest& request)
{
  if (!std::filesystem::exists(request.model_path)) {
    return asryx::fail("model file does not exist: " + request.model_path);
  }

  if (!std::filesystem::exists(request.vad_model_path)) {
    return asryx::fail("VAD model file does not exist: " + request.vad_model_path);
  }

  return {};
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

whisper_context_params _context_params()
{
  whisper_context_params params = whisper_context_default_params();

  if constexpr (compute::kCompiledBackend == compute::CompiledBackend::Cpu) {
    params.use_gpu = false;
  }
  else {
    params.use_gpu = true;
    params.gpu_device = 0;
  }

  return params;
}

std::expected<WhisperContext, asryx::Error> _load_context(const std::string& model_path)
{
  const auto context_params = _context_params();
  WhisperContext ctx(whisper_init_from_file_with_params(model_path.c_str(), context_params));

  if (ctx == nullptr) {
    return asryx::fail("failed to initialize whisper model: " + model_path);
  }

  return ctx;
}

whisper_full_params _params(whisper_context* ctx, TranscriptionRequest& request, bool use_vad)
{
  whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

  params.temperature = 0.0F;
  params.temperature_inc = 0.2F;

  params.greedy.best_of = 5;
  params.no_context = true;

  params.n_threads = compute::resolve_threads();
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.no_timestamps = true;

  params.suppress_blank = true;
  params.suppress_nst = false;

  params.language = _language(ctx, request.language);
  params.detect_language = false;

  params.vad = use_vad;
  params.vad_model_path = use_vad ? request.vad_model_path.c_str() : nullptr;
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

std::vector<std::string> _normalized_words(std::string_view text)
{
  std::vector<std::string> words;
  std::string word;

  for (const char character : text) {
    const auto byte = static_cast<unsigned char>(character);

    if (std::isalnum(byte) != 0 || character == '\'') {
      word.push_back(static_cast<char>(std::tolower(byte)));
      continue;
    }

    if (!word.empty()) {
      words.push_back(std::move(word));
      word.clear();
    }
  }

  if (!word.empty()) {
    words.push_back(std::move(word));
  }

  return words;
}

bool _all_words_equal(const std::vector<std::string>& words)
{
  return words.size() >= 2 &&
         std::all_of(words.begin() + 1, words.end(), [&words](const std::string& word) {
           return word == words.front();
         });
}

bool _looks_collapsed(const std::string& output, size_t sample_count)
{
  const double audio_seconds =
      static_cast<double>(sample_count) / static_cast<double>(WHISPER_SAMPLE_RATE);

  if (audio_seconds < COLLAPSE_MIN_AUDIO_SECONDS) {
    return false;
  }

  const auto words = _normalized_words(output);
  return words.size() <= COLLAPSE_MAX_WORDS || _all_words_equal(words);
}

std::expected<std::string, asryx::Error>
_transcribe_once(const TranscriptionRequest& request, const std::vector<float>& samples,
                 bool use_vad)
{
  const auto ctx = _load_context(request.model_path);
  if (!ctx) {
    return std::unexpected(ctx.error());
  }

  auto transcription_request = request;
  const auto params = _params(ctx->get(), transcription_request, use_vad);

  if (whisper_full(ctx->get(), params, samples.data(), static_cast<int>(samples.size())) != 0) {
    if (!request.cancel_marker_path.empty() && std::filesystem::exists(request.cancel_marker_path))
    {
      return asryx::fail("transcription canceled");
    }

    return asryx::fail("transcription failed");
  }

  return _read_output(ctx->get());
}

const std::string& _better_output(const std::string& primary, const std::string& retry)
{
  const auto primary_words = _normalized_words(primary);
  const auto retry_words = _normalized_words(retry);

  if (retry_words.size() > primary_words.size()) {
    return retry;
  }

  if (retry_words.size() == primary_words.size() && retry.size() > primary.size()) {
    return retry;
  }

  return primary;
}

} // namespace

std::expected<std::string, asryx::Error> run(const TranscriptionRequest& request)
{
  const auto valid_request = _validate_request(request);
  if (!valid_request) {
    return std::unexpected(valid_request.error());
  }

  const auto samples = audio::read_pcm16_wav(request.wav_path);
  if (!samples) {
    return std::unexpected(samples.error());
  }

  const auto primary = _transcribe_once(request, *samples, true);
  if (!primary) {
    return std::unexpected(primary.error());
  }

  if (!_looks_collapsed(*primary, samples->size())) {
    return *primary;
  }

  const auto retry = _transcribe_once(request, *samples, false);
  if (!retry) {
    if (!request.cancel_marker_path.empty() && std::filesystem::exists(request.cancel_marker_path))
    {
      return std::unexpected(retry.error());
    }

    return *primary;
  }

  return _better_output(*primary, *retry);
}

} // namespace engine::transcription
