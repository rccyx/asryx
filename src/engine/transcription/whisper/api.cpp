#include "engine/transcription/whisper/api.hpp"

#include "constants/constants.hpp"
#include "engine/transcription/inference/backend.hpp"

#include <algorithm>
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

_vad_segments _speech_segments(const SpeechInput& input)
{
  const _vad_context ctx(whisper_vad_init_from_file_with_params(
      input.vad_model_path.c_str(), whisper_vad_default_context_params()));
  if (!ctx) {
    return {};
  }

  return _vad_segments(whisper_vad_segments_from_samples(ctx.get(), whisper_vad_default_params(),
                                                         input.samples.data(),
                                                         static_cast<int>(input.samples.size())));
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
  params.temperature_inc = input.fast_decode ? 0.0F : 0.2F;
  params.greedy.best_of = input.fast_decode ? 1 : 5;
  params.no_context = true;

  params.n_threads = inference::resolve_threads();
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.no_timestamps = false;
  params.token_timestamps = input.token_timestamps;

  params.suppress_blank = true;
  params.suppress_nst = true;

  if (input.permissive_no_speech) {
    params.no_speech_thold = 1.01F;
  }

  params.language = _language(ctx, input.request->language);
  params.detect_language = false;

  const std::string& active_prompt =
      input.prompt_override.empty() ? input.request->prompt : input.prompt_override;
  params.initial_prompt = active_prompt.empty() ? nullptr : active_prompt.c_str();

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
    return yx::fail(yx::ErrorCode::transcription,
                    "failed to initialize whisper model: " + model_path);
  }

  return yx::ok(Context(std::move(state)));
}

yx::Result<int> prompt_token_count(const Context& ctx, const std::string& prompt)
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

std::vector<Word> read_words(const Context& ctx)
{
  auto* const raw = ctx._state->ctx.get();
  const whisper_token eot = whisper_token_eot(raw);
  const int n_segments = whisper_full_n_segments(raw);
  std::vector<Word> words;

  for (int s = 0; s < n_segments; ++s) {
    const std::int64_t seg_t0 = std::max<std::int64_t>(0, whisper_full_get_segment_t0(raw, s));
    const std::int64_t seg_t1 = std::max<std::int64_t>(seg_t0, whisper_full_get_segment_t1(raw, s));
    const int n_tokens = whisper_full_n_tokens(raw, s);

    std::vector<Word> seg_words;
    bool has_token_ts = false;

    for (int t = 0; t < n_tokens; ++t) {
      const whisper_token_data data = whisper_full_get_token_data(raw, s, t);
      if (data.id >= eot) {
        continue;
      }

      const char* const raw_text = whisper_full_get_token_text(raw, s, t);
      if (raw_text == nullptr || raw_text[0] == '\0') {
        continue;
      }

      if (data.t1 > data.t0 || data.t0 > seg_t0) {
        has_token_ts = true;
      }

      std::string piece(raw_text);
      const std::int64_t t0 = std::clamp<std::int64_t>(data.t0, seg_t0, seg_t1);
      const std::int64_t t1 = std::clamp<std::int64_t>(data.t1, t0, seg_t1);

      const bool starts_word =
          seg_words.empty() || static_cast<unsigned char>(piece.front()) <= 0x20U;
      if (starts_word) {
        if (seg_words.empty() && !words.empty() &&
            static_cast<unsigned char>(piece.front()) > 0x20U)
        {
          piece.insert(piece.begin(), ' ');
        }
        seg_words.push_back({t0, t1, std::move(piece)});
      }
      else {
        seg_words.back().t1_cs = std::max(seg_words.back().t1_cs, t1);
        seg_words.back().text += piece;
      }
    }

    if (!seg_words.empty() && !has_token_ts && seg_t1 > seg_t0) {
      std::size_t total_chars = 0;
      for (const auto& w : seg_words) {
        total_chars += w.text.size();
      }
      if (total_chars > 0) {
        std::size_t cursor = 0;
        const std::int64_t span = seg_t1 - seg_t0;
        for (auto& w : seg_words) {
          w.t0_cs = seg_t0 + (span * static_cast<std::int64_t>(cursor)) /
                                 static_cast<std::int64_t>(total_chars);
          cursor += w.text.size();
          w.t1_cs = seg_t0 + (span * static_cast<std::int64_t>(cursor)) /
                                 static_cast<std::int64_t>(total_chars);
        }
      }
    }

    for (auto& w : seg_words) {
      words.push_back(std::move(w));
    }
  }

  return words;
}

double detected_speech_seconds(const SpeechInput& input)
{
  const auto segments = _speech_segments(input);
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

yx::Result<std::size_t> speech_end(const SpeechInput& input)
{
  const auto segments = _speech_segments(input);
  if (!segments) {
    return yx::fail(yx::ErrorCode::transcription, "cannot detect speech in rolling audio");
  }

  const int count = whisper_vad_segments_n_segments(segments.get());
  if (count == 0) {
    return yx::ok<std::size_t>(0U);
  }

  // keep a short tail so a cut at the vad boundary does not clip the last word.
  constexpr float TAIL_CS = 25.0F;
  const auto end = whisper_vad_segments_get_segment_t1(segments.get(), count - 1);
  const auto samples = static_cast<std::size_t>((end + TAIL_CS) * (WHISPER_SAMPLE_RATE / 100));
  return yx::ok(std::min(samples, input.samples.size()));
}

} // namespace engine::transcription::whisper
