#include "engine/engine.hpp"

#include "constants/constants.hpp"
#include "platform/process.hpp"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
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

namespace engine {

namespace {

#ifdef ASRYX_TESTING
#  define ASRYX_TEST_HOOK(name, ...)                                                               \
    if (auto hook = testing::name()) {                                                             \
      return hook(__VA_ARGS__);                                                                    \
    }
#else
#  define ASRYX_TEST_HOOK(name, ...)
#endif

constexpr std::uint16_t WAV_PCM = 1;
constexpr std::uint16_t WAV_EXTENSIBLE = 65534;
constexpr float PCM16_SCALE = 32768.0F;

bool _chunk_is(const std::vector<std::uint8_t>& bytes, size_t offset, const char* id)
{
  return offset + 4 <= bytes.size() && std::memcmp(bytes.data() + offset, id, 4) == 0;
}

std::uint16_t _read_u16_le(const std::vector<std::uint8_t>& bytes, size_t offset)
{
  if (offset + 2 > bytes.size()) {
    throw std::runtime_error("invalid wav header");
  }

  return static_cast<std::uint16_t>(bytes[offset]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t _read_u32_le(const std::vector<std::uint8_t>& bytes, size_t offset)
{
  if (offset + 4 > bytes.size()) {
    throw std::runtime_error("invalid wav header");
  }

  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::vector<std::uint8_t> _read_file(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open wav file: " + path);
  }

  std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(file),
                                  std::istreambuf_iterator<char>{});
  return bytes;
}

void _validate_wav_format(std::uint16_t audio_format, std::uint16_t channels,
                          std::uint32_t sample_rate, std::uint16_t bits_per_sample)
{
  const bool supported_format = audio_format == WAV_PCM || audio_format == WAV_EXTENSIBLE;
  if (!supported_format) {
    throw std::runtime_error("unsupported wav format: expected PCM");
  }

  if (channels != 1 || sample_rate != WHISPER_SAMPLE_RATE || bits_per_sample != 16) {
    throw std::runtime_error("unsupported wav format: expected 16 kHz mono s16 PCM");
  }
}

std::vector<float> _decode_pcm16(const std::vector<std::uint8_t>& bytes, size_t data_offset,
                                 size_t data_size)
{
  const size_t aligned_data_size = data_size - (data_size % 2U);

  std::vector<float> samples;
  samples.reserve(aligned_data_size / 2U);

  for (size_t i = data_offset; i < data_offset + aligned_data_size; i += 2) {
    const auto high = static_cast<std::uint16_t>(bytes[i + 1]);
    const auto low = static_cast<std::uint16_t>(bytes[i]);
    const auto raw = static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8U));
    const auto sample = static_cast<std::int16_t>(raw);
    samples.push_back(static_cast<float>(sample) / PCM16_SCALE);
  }

  return samples;
}

std::vector<float> _read_pcm16_wav(const std::string& path)
{
  const auto bytes = _read_file(path);

  if (bytes.size() < 44 || !_chunk_is(bytes, 0, "RIFF") || !_chunk_is(bytes, 8, "WAVE")) {
    throw std::runtime_error("unsupported wav file: expected RIFF/WAVE");
  }

  bool found_fmt = false;
  bool found_sample_data = false;

  std::uint16_t audio_format = 0;
  std::uint16_t channels = 0;
  std::uint32_t sample_rate = 0;
  std::uint16_t bits_per_sample = 0;

  size_t data_offset = 0;
  size_t data_size = 0;

  size_t offset = 12;
  while (offset + 8 <= bytes.size()) {
    const std::uint32_t declared_size = _read_u32_le(bytes, offset + 4);
    const size_t chunk_data_offset = offset + 8;
    const size_t remaining_bytes = bytes.size() - chunk_data_offset;

    if (_chunk_is(bytes, offset, "fmt ")) {
      if (declared_size > remaining_bytes || declared_size < 16) {
        throw std::runtime_error("invalid wav fmt chunk");
      }

      audio_format = _read_u16_le(bytes, chunk_data_offset);
      channels = _read_u16_le(bytes, chunk_data_offset + 2);
      sample_rate = _read_u32_le(bytes, chunk_data_offset + 4);
      bits_per_sample = _read_u16_le(bytes, chunk_data_offset + 14);
      found_fmt = true;
    }
    else if (_chunk_is(bytes, offset, "data")) {
      data_offset = chunk_data_offset;
      if (declared_size == 0 || declared_size > remaining_bytes) {
        data_size = remaining_bytes;
      }
      else {
        data_size = static_cast<size_t>(declared_size);
      }
      found_sample_data = true;
      break;
    }

    if (declared_size > remaining_bytes) {
      break;
    }

    offset = chunk_data_offset + declared_size + (declared_size % 2U);
  }

  if (!found_fmt || !found_sample_data) {
    throw std::runtime_error("invalid wav file: missing fmt or data chunk");
  }

  _validate_wav_format(audio_format, channels, sample_rate, bits_per_sample);
  return _decode_pcm16(bytes, data_offset, data_size);
}

bool _wait_until_recorder_exits(pid_t pid)
{
  for (int attempt = 0; attempt < 100; ++attempt) {
    int status = 0;
    const pid_t result = waitpid(pid, &status, WNOHANG);
    if (result == pid) {
      return true;
    }

    if (result == -1) {
      if (errno != ECHILD) {
        return false;
      }

      if (!platform::is_process_running(pid)) {
        return true;
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  return false;
}

int _thread_count()
{
  const auto detected = std::thread::hardware_concurrency();
  if (detected == 0) {
    return 4;
  }

  return static_cast<int>(std::min(4U, detected));
}

const char* _whisper_language(whisper_context* ctx, const std::string& language)
{
  if (whisper_is_multilingual(ctx) == 0) {
    return constants::config::english_language.data();
  }

  if (language.empty() || language == constants::config::auto_language) {
    return nullptr;
  }

  return language.c_str();
}

std::vector<std::string> _recorder_args(const std::string& wav_path)
{
  if (platform::command_exists("pw-record")) {
    return {"pw-record", "--format=s16", "--rate=16000", "--channels=1", wav_path};
  }

  if (platform::command_exists("arecord")) {
    return {"arecord", "-q", "-t", "wav", "-f", "S16_LE", "-c", "1", "-r", "16000", wav_path};
  }

  throw std::runtime_error("No recorder tool found (need pw-record or arecord)");
}

struct WhisperContextDeleter
{
  void operator()(whisper_context* ctx) const
  {
    if (ctx != nullptr) {
      whisper_free(ctx);
    }
  }
};

} // namespace

pid_t start_recording(const std::string& wav_path, const std::string& err_path)
{
  ASRYX_TEST_HOOK(start_recording_hook, wav_path, err_path);

  const auto args = _recorder_args(wav_path);
  const pid_t pid = platform::spawn_process_background(args, err_path);
  if (pid == -1) {
    throw std::runtime_error("Failed to start recorder process");
  }

  return pid;
}

bool stop_recording(pid_t pid)
{
  ASRYX_TEST_HOOK(stop_recording_hook, pid);

  if (pid <= 0) {
    return false;
  }

  platform::stop_process(pid, SIGINT);
  if (_wait_until_recorder_exits(pid)) {
    return true;
  }

  platform::stop_process(pid, SIGTERM);
  if (_wait_until_recorder_exits(pid)) {
    return true;
  }

  platform::stop_process(pid, SIGKILL);
  return _wait_until_recorder_exits(pid);
}

std::string transcribe(const std::string& model_path, const std::string& wav_path,
                       const std::string& language)
{
  ASRYX_TEST_HOOK(transcribe_hook, model_path, wav_path, language);

  if (!std::filesystem::exists(model_path)) {
    throw std::runtime_error("model file does not exist: " + model_path);
  }

  const auto samples = _read_pcm16_wav(wav_path);

  whisper_context_params context_params = whisper_context_default_params();
  std::unique_ptr<whisper_context, WhisperContextDeleter> ctx(
      whisper_init_from_file_with_params(model_path.c_str(), context_params));

  if (ctx == nullptr) {
    throw std::runtime_error("failed to initialize whisper model: " + model_path);
  }

  const char* const language_arg = _whisper_language(ctx.get(), language);

  whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  params.n_threads = _thread_count();
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.no_timestamps = true;
  params.suppress_blank = true;
  params.suppress_nst = true;
  params.language = language_arg;
  params.detect_language = false;

  if (whisper_full(ctx.get(), params, samples.data(), static_cast<int>(samples.size())) != 0) {
    throw std::runtime_error("whisper transcription failed");
  }

  std::string output;
  const int segments = whisper_full_n_segments(ctx.get());

  for (int i = 0; i < segments; ++i) {
    const char* const text = whisper_full_get_segment_text(ctx.get(), i);
    if (text != nullptr) {
      output += text;
    }
  }

  return output;
}

} // namespace engine
