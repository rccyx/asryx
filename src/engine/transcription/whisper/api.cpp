#include "engine/transcription/whisper/api.hpp"

#include "constants/constants.hpp"
#include "engine/transcription/inference/backend.hpp"

#include <filesystem>
#include <string>
#include <utility>

#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#endif

#include <whisper.h>

#ifdef __GNUC__
#  pragma GCC diagnostic pop
#endif

namespace engine::transcription::whisper {

namespace {

struct _context_deleter
{
  void operator()(whisper_context* ctx) const
  {
    if (ctx != nullptr) {
      whisper_free(ctx);
    }
  }
};

using _raw_context = std::unique_ptr<whisper_context, _context_deleter>;

struct _vad_context_deleter
{
  void operator()(whisper_vad_context* vctx) const
  {
    if (vctx != nullptr) {
      whisper_vad_free(vctx);
    }
  }
};

using _vad_context = std::unique_ptr<whisper_vad_context, _vad_context_deleter>;

struct _vad_segments_deleter
{
  void operator()(whisper_vad_segments* segments) const
  {
    if (segments != nullptr) {
      whisper_vad_free_segments(segments);
    }
  }
};

using _vad_segments = std::unique_ptr<whisper_vad_segments, _vad_segments_deleter>;

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

void _silence_logs(ggml_log_level level, const char* text, void* user_data)
{
  (void)level;
  (void)text;
  (void)user_data;
}

whisper_context_params _context_params()
{
  whisper_context_params params = whisper_context_default_params();

  if (inference::uses_gpu()) {
    params.use_gpu = true;
    params.gpu_device = 0;
  }
  else {
    params.use_gpu = false;
  }

  return params;
}

whisper_full_params _transcription_params(const TranscribeInput& input, whisper_context* ctx)
{
  whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);

  params.temperature = 0.0F;
  params.temperature_inc = 0.2F;
  params.greedy.best_of = 5;
  params.no_context = true;

  params.n_threads = inference::resolve_threads();
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.no_timestamps = false;

  params.suppress_blank = true;
  params.suppress_nst = true;

  if (input.permissive_no_speech) {
    params.no_speech_thold = 1.01F;
  }

  params.language = _language(ctx, input.request->language);
  params.detect_language = false;
  params.initial_prompt = input.request->prompt.empty() ? nullptr : input.request->prompt.c_str();

  params.vad = input.use_vad;
  params.vad_model_path = input.request->vad_model_path.c_str();
  params.vad_params = whisper_vad_default_params();

  params.abort_callback = _abort_requested;
  params.abort_callback_user_data = &input.request->cancel_marker_path;

  return params;
}

} // namespace

struct _context_state
{
  _raw_context ctx;
};

Context::Context(Context&& other) noexcept = default;

Context::~Context() = default;

Context& Context::operator=(Context&& other) noexcept = default;

Context::Context(std::unique_ptr<_context_state> state)
    : _state(std::move(state))
{
}

yx::Result<Context> load_context(const std::string& model_path)
{
  whisper_log_set(_silence_logs, nullptr);

  const auto context_params = _context_params();
  auto state = std::make_unique<_context_state>();
  state->ctx.reset(whisper_init_from_file_with_params(model_path.c_str(), context_params));

  if (state->ctx == nullptr) {
    return yx::fail("failed to initialize whisper model: " + model_path);
  }

  return yx::ok(Context(std::move(state)));
}

yx::Result<int> prompt_token_count(Context& ctx, const std::string& prompt)
{
  whisper_context* const raw_ctx = ctx._state->ctx.get();
  const int count = whisper_tokenize(raw_ctx, prompt.c_str(), nullptr, 0);
  if (count > 0) {
    return yx::ok(count);
  }

  return yx::ok(-count);
}

bool transcribe(const TranscribeInput& input)
{
  whisper_context* const ctx = input.ctx->_state->ctx.get();
  auto params = _transcription_params(input, ctx);
  return whisper_full(ctx, params, input.samples.data(), static_cast<int>(input.samples.size())) ==
         0;
}

std::string read_output(const Context& ctx)
{
  std::string output;
  const int segments = whisper_full_n_segments(ctx._state->ctx.get());

  for (int i = 0; i < segments; ++i) {
    const char* const text = whisper_full_get_segment_text(ctx._state->ctx.get(), i);

    if (text != nullptr) {
      output += text;
    }
  }

  return output;
}

double detected_speech_seconds(const SpeechInput& input)
{
  const whisper_vad_context_params vad_ctx_params = whisper_vad_default_context_params();
  _vad_context vctx(
      whisper_vad_init_from_file_with_params(input.vad_model_path.c_str(), vad_ctx_params));

  if (!vctx) {
    return 0.0;
  }

  const whisper_vad_params vad_params = whisper_vad_default_params();
  _vad_segments segments(whisper_vad_segments_from_samples(
      vctx.get(), vad_params, input.samples.data(), static_cast<int>(input.samples.size())));

  if (!segments) {
    return 0.0;
  }

  double total_cs = 0.0;
  const int n = whisper_vad_segments_n_segments(segments.get());

  for (int i = 0; i < n; ++i) {
    total_cs += static_cast<double>(whisper_vad_segments_get_segment_t1(segments.get(), i) -
                                    whisper_vad_segments_get_segment_t0(segments.get(), i));
  }

  return total_cs / 100.0;
}

} // namespace engine::transcription::whisper
