#ifndef ASRYX_ENGINE_TRANSCRIPTION_WHISPER_API_HPP
#define ASRYX_ENGINE_TRANSCRIPTION_WHISPER_API_HPP

#include "engine/engine.hpp"
#include "error.hpp"

#include <memory>
#include <span>
#include <string>

namespace engine::transcription::whisper {

struct _context_state;
struct TranscribeInput;

class Context final
{
public:
  Context(Context&& other) noexcept;
  Context(const Context& other) = delete;
  ~Context();

  Context& operator=(Context&& other) noexcept;
  Context& operator=(const Context& other) = delete;

private:
  explicit Context(std::unique_ptr<_context_state> state);

  friend yx::Result<Context> load_context(const std::string& model_path);
  friend bool transcribe(const TranscribeInput& input);
  friend std::string read_output(const Context& ctx);

  std::unique_ptr<_context_state> _state;
};

struct TranscribeInput
{
  Context* ctx;
  TranscriptionRequest* request;
  std::span<const float> samples;
  bool use_vad;
  bool permissive_no_speech;
};

struct SpeechInput
{
  std::string vad_model_path;
  std::span<const float> samples;
};

yx::Result<Context> load_context(const std::string& model_path);
bool transcribe(const TranscribeInput& input);
std::string read_output(const Context& ctx);
double detected_speech_seconds(const SpeechInput& input);

} // namespace engine::transcription::whisper

#endif // ASRYX_ENGINE_TRANSCRIPTION_WHISPER_API_HPP
