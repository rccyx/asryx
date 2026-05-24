#include "engine/engine.hpp"

#include "constants/constants.hpp"
#include "platform/process.hpp"

#include <algorithm>
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

constexpr std::uint16_t wav_pcm = 1;
constexpr std::uint16_t wav_extensible = 65534;
constexpr float pcm16_scale = 32768.0F;

bool chunk_is(const std::vector<std::uint8_t>& bytes, size_t offset, const char* id)
{
  return offset + 4 <= bytes.size() && std::memcmp(bytes.data() + offset, id, 4) == 0;
}

std::uint16_t read_u16_le(const std::vector<std::uint8_t>& bytes, size_t offset)
{
  if (offset + 2 > bytes.size()) {
    throw std::runtime_error("invalid wav header");
  }

  return static_cast<std::uint16_t>(bytes[offset]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t read_u32_le(const std::vector<std::uint8_t>& bytes, size_t offset)
{
  if (offset + 4 > bytes.size()) {
    throw std::runtime_error("invalid wav header");
  }

  return static_cast<std::uint32_t>(bytes[offset]) |
         (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::vector<std::uint8_t> read_file(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open wav file: " + path);
  }

  std::vector<std::uint8_t> bytes(std::istreambuf_iterator<char>(file),
                                  std::istreambuf_iterator<char>{});
  return bytes;
}

void validate_wav_format(std::uint16_t audio_format, std::uint16_t channels,
                         std::uint32_t sample_rate, std::uint16_t bits_per_sample)
{
  const bool supported_format = audio_format == wav_pcm || audio_format == wav_extensible;
  if (!supported_format) {
    throw std::runtime_error("unsupported wav format: expected PCM");
  }

  if (channels != 1 || sample_rate != WHISPER_SAMPLE_RATE || bits_per_sample != 16) {
    throw std::runtime_error("unsupported wav format: expected 16 kHz mono s16 PCM");
  }
}

std::vector<float> decode_pcm16(const std::vector<std::uint8_t>& bytes, size_t data_offset,
                                size_t data_size)
{
  data_size -= data_size % 2U;

  std::vector<float> samples;
  samples.reserve(data_size / 2U);

  for (size_t i = data_offset; i < data_offset + data_size; i += 2) {
    const auto high = static_cast<std::uint16_t>(bytes[i + 1]);
    const auto low = static_cast<std::uint16_t>(bytes[i]);
    const auto raw = static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8U));
    const auto sample = static_cast<std::int16_t>(raw);
    samples.push_back(static_cast<float>(sample) / pcm16_scale);
  }

  return samples;
}

std::vector<float> read_pcm16_wav(const std::string& path)
{
  const auto bytes = read_file(path);

  if (bytes.size() < 44 || !chunk_is(bytes, 0, "RIFF") || !chunk_is(bytes, 8, "WAVE")) {
    throw std::runtime_error("unsupported wav file: expected RIFF/WAVE");
  }

  bool found_fmt = false;
  bool found_data = false;

  std::uint16_t audio_format = 0;
  std::uint16_t channels = 0;
  std::uint32_t sample_rate = 0;
  std::uint16_t bits_per_sample = 0;

  size_t data_offset = 0;
  size_t data_size = 0;

  size_t offset = 12;
  while (offset + 8 <= bytes.size()) {
    const std::uint32_t declared_size = read_u32_le(bytes, offset + 4);
    const size_t chunk_data = offset + 8;
    const size_t remaining = bytes.size() - chunk_data;

    if (chunk_is(bytes, offset, "fmt ")) {
      if (declared_size > remaining || declared_size < 16) {
        throw std::runtime_error("invalid wav fmt chunk");
      }

      audio_format = read_u16_le(bytes, chunk_data);
      channels = read_u16_le(bytes, chunk_data + 2);
      sample_rate = read_u32_le(bytes, chunk_data + 4);
      bits_per_sample = read_u16_le(bytes, chunk_data + 14);
      found_fmt = true;
    }
    else if (chunk_is(bytes, offset, "data")) {
      data_offset = chunk_data;
      if (declared_size == 0 || declared_size > remaining) {
        data_size = remaining;
      }
      else {
        data_size = static_cast<size_t>(declared_size);
      }
      found_data = true;
      break;
    }

    if (declared_size > remaining) {
      break;
    }

    offset = chunk_data + declared_size + (declared_size % 2U);
  }

  if (!found_fmt || !found_data) {
    throw std::runtime_error("invalid wav file: missing fmt or data chunk");
  }

  validate_wav_format(audio_format, channels, sample_rate, bits_per_sample);
  return decode_pcm16(bytes, data_offset, data_size);
}

bool wait_until_process_exits(pid_t pid)
{
  for (int attempt = 0; attempt < 100; ++attempt) {
    if (!platform::is_process_running(pid)) {
      return true;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  return false;
}

int thread_count()
{
  const auto detected = std::thread::hardware_concurrency();
  if (detected == 0) {
    return 4;
  }

  return static_cast<int>(std::min(4U, detected));
}

const char* whisper_language(whisper_context* ctx, const std::string& language)
{
  if (whisper_is_multilingual(ctx) == 0) {
    return constants::config::english_language.data();
  }

  if (language.empty() || language == constants::config::auto_language) {
    return nullptr;
  }

  return language.c_str();
}

struct WhisperDeleter
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
  std::vector<std::string> args;
  if (platform::command_exists("pw-record")) {
    args = {"pw-record", "--format=s16", "--rate=16000", "--channels=1", wav_path};
  }
  else if (platform::command_exists("arecord")) {
    args = {"arecord", "-q", "-t", "wav", "-f", "S16_LE", "-c", "1", "-r", "16000", wav_path};
  }
  else {
    throw std::runtime_error("No recorder tool found (need pw-record or arecord)");
  }

  pid_t pid = platform::spawn_process_background(args, err_path);
  if (pid == -1) {
    throw std::runtime_error("Failed to start recorder process");
  }

  return pid;
}

bool stop_recording(pid_t pid)
{
  if (pid <= 0) {
    return false;
  }

  platform::stop_process(pid, SIGINT);
  if (wait_until_process_exits(pid)) {
    return true;
  }

  platform::stop_process(pid, SIGTERM);
  if (wait_until_process_exits(pid)) {
    return true;
  }

  platform::stop_process(pid, SIGKILL);
  return wait_until_process_exits(pid);
}

std::string transcribe(const std::string& model_path, const std::string& wav_path,
                       const std::string& language)
{
  if (!std::filesystem::exists(model_path)) {
    throw std::runtime_error("model file does not exist: " + model_path);
  }

  auto samples = read_pcm16_wav(wav_path);

  whisper_context_params context_params = whisper_context_default_params();
  std::unique_ptr<whisper_context, WhisperDeleter> ctx(
      whisper_init_from_file_with_params(model_path.c_str(), context_params));

  if (ctx == nullptr) {
    throw std::runtime_error("failed to initialize whisper model: " + model_path);
  }

  const char* language_arg = whisper_language(ctx.get(), language);

  whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  params.n_threads = thread_count();
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.no_timestamps = true;
  params.language = language_arg;
  params.detect_language = false;

  if (whisper_full(ctx.get(), params, samples.data(), static_cast<int>(samples.size())) != 0) {
    throw std::runtime_error("whisper transcription failed");
  }

  std::string output;
  const int segments = whisper_full_n_segments(ctx.get());

  for (int i = 0; i < segments; ++i) {
    const char* text = whisper_full_get_segment_text(ctx.get(), i);
    if (text != nullptr) {
      output += text;
    }
  }

  return output;
}

bool copy_to_clipboard(const std::string& text)
{
  if (platform::command_exists("wl-copy")) {
    return platform::run_process_with_stdin({"wl-copy"}, text);
  }

  if (platform::command_exists("xclip")) {
    return platform::run_process_with_stdin({"xclip", "-selection", "clipboard"}, text);
  }

  std::cerr << "Warning: Neither wl-copy nor xclip is available to copy transcript.\n";
  return false;
}

bool send_notification(const std::string& message)
{
  if (platform::command_exists("notify-send")) {
    return platform::run_process_blocking(
        {"notify-send", std::string(constants::app_name), message});
  }

  return false;
}

} // namespace engine
