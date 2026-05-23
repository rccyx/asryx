#include "engine/engine.hpp"

#include "platform/process.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef ASRYX_TESTING
#  include <whisper.h>
#endif

namespace engine {

#ifndef ASRYX_TESTING
namespace {

bool is_chunk(const std::vector<std::uint8_t>& bytes, size_t offset, const char* id)
{
  return offset + 4U <= bytes.size() && std::memcmp(bytes.data() + offset, id, 4U) == 0;
}

std::uint16_t read_u16_le(const std::vector<std::uint8_t>& bytes, size_t offset)
{
  if (offset + 2U > bytes.size()) {
    throw std::runtime_error("invalid wav header");
  }

  const auto low = static_cast<std::uint16_t>(bytes[offset]);
  const auto high = static_cast<std::uint16_t>(bytes[offset + 1U]);
  return static_cast<std::uint16_t>(low | static_cast<std::uint16_t>(high << 8U));
}

std::uint32_t read_u32_le(const std::vector<std::uint8_t>& bytes, size_t offset)
{
  if (offset + 4U > bytes.size()) {
    throw std::runtime_error("invalid wav header");
  }

  const auto b0 = static_cast<std::uint32_t>(bytes[offset]);
  const auto b1 = static_cast<std::uint32_t>(bytes[offset + 1U]);
  const auto b2 = static_cast<std::uint32_t>(bytes[offset + 2U]);
  const auto b3 = static_cast<std::uint32_t>(bytes[offset + 3U]);

  return b0 | (b1 << 8U) | (b2 << 16U) | (b3 << 24U);
}

std::vector<std::uint8_t> read_file(const std::string& path)
{
  std::ifstream file(path, std::ios::binary);
  if (!file.is_open()) {
    throw std::runtime_error("failed to open wav file: " + path);
  }

  return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

float pcm16_to_float(std::uint16_t raw)
{
  auto sample = static_cast<std::int32_t>(raw);
  if (sample >= 32768) {
    sample -= 65536;
  }

  return static_cast<float>(sample) / 32768.0F;
}

void read_fmt_chunk(const std::vector<std::uint8_t>& bytes, size_t chunk_data,
                    std::uint16_t& audio_format, std::uint16_t& channels,
                    std::uint32_t& sample_rate, std::uint16_t& bits_per_sample)
{
  audio_format = read_u16_le(bytes, chunk_data);
  channels = read_u16_le(bytes, chunk_data + 2U);
  sample_rate = read_u32_le(bytes, chunk_data + 4U);
  bits_per_sample = read_u16_le(bytes, chunk_data + 14U);
}

std::vector<float> read_pcm16_wav(const std::string& path)
{
  const auto bytes = read_file(path);

  if (bytes.size() < 44U || !is_chunk(bytes, 0U, "RIFF") || !is_chunk(bytes, 8U, "WAVE")) {
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

  while (offset + 8U <= bytes.size()) {
    const auto chunk_size = static_cast<size_t>(read_u32_le(bytes, offset + 4U));
    const size_t chunk_data = offset + 8U;

    if (chunk_size > bytes.size() - chunk_data) {
      throw std::runtime_error("invalid wav chunk size");
    }

    if (is_chunk(bytes, offset, "fmt ")) {
      if (chunk_size < 16U) {
        throw std::runtime_error("invalid wav fmt chunk");
      }
      read_fmt_chunk(bytes, chunk_data, audio_format, channels, sample_rate, bits_per_sample);
      found_fmt = true;
    }
    else if (is_chunk(bytes, offset, "data")) {
      data_offset = chunk_data;
      data_size = chunk_size;
      found_data = true;
    }

    offset = chunk_data + chunk_size + (chunk_size % 2U);
  }

  if (!found_fmt || !found_data) {
    throw std::runtime_error("invalid wav file: missing fmt or data chunk");
  }

  if (audio_format != 1U) {
    throw std::runtime_error("unsupported wav format: expected PCM");
  }

  if (channels != 1U || sample_rate != static_cast<std::uint32_t>(WHISPER_SAMPLE_RATE) ||
      bits_per_sample != 16U) {
    throw std::runtime_error("unsupported wav format: expected 16 kHz mono s16 PCM");
  }

  if (data_size % 2U != 0U) {
    throw std::runtime_error("invalid wav data size");
  }

  std::vector<float> samples;
  samples.reserve(data_size / 2U);

  for (size_t i = data_offset; i < data_offset + data_size; i += 2U) {
    samples.push_back(pcm16_to_float(read_u16_le(bytes, i)));
  }

  return samples;
}

int thread_count()
{
  const unsigned int detected = std::thread::hardware_concurrency();
  if (detected == 0U) {
    return 4;
  }

  return static_cast<int>(std::min(4U, detected));
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
#endif

pid_t start_recording(const std::string& wav_path, const std::string& err_path)
{
  std::vector<std::string> args;
  if (platform::command_exists("pw-record")) {
    args = {"pw-record", "--format=s16", "--rate=16000", "--channels=1", wav_path};
  }
  else if (platform::command_exists("arecord")) {
    args = {"arecord", "-f", "S16_LE", "-c", "1", "-r", "16000", wav_path};
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
  platform::stop_process(pid, 2);
  platform::wait_process(pid);
  return true;
}

std::string transcribe(const std::string& model_path, const std::string& wav_path)
{
#ifdef ASRYX_TESTING
  static_cast<void>(model_path);
  static_cast<void>(wav_path);
  return "Test transcription.\n";
#else
  if (!std::filesystem::exists(model_path)) {
    throw std::runtime_error("model file does not exist: " + model_path);
  }

  auto samples = read_pcm16_wav(wav_path);
  if (samples.size() > static_cast<size_t>(std::numeric_limits<int>::max())) {
    throw std::runtime_error("wav file is too large for whisper");
  }

  whisper_context_params context_params = whisper_context_default_params();
  context_params.use_gpu = false;

  std::unique_ptr<whisper_context, WhisperContextDeleter> ctx(
      whisper_init_from_file_with_params(model_path.c_str(), context_params));

  if (ctx == nullptr) {
    throw std::runtime_error("failed to initialize whisper model: " + model_path);
  }

  whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
  params.n_threads = thread_count();
  params.print_progress = false;
  params.print_realtime = false;
  params.print_timestamps = false;
  params.no_timestamps = true;

  const int sample_count = static_cast<int>(samples.size());
  if (whisper_full(ctx.get(), params, samples.data(), sample_count) != 0) {
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
#endif
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
    return platform::run_process_blocking({"notify-send", "asryx", message});
  }
  return false;
}

} // namespace engine
