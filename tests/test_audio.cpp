#include "tests/audio/wav_fixture.hpp"
#include "tests/tests.hpp"

#include <cstdint>
#include <iostream>
#include <libassert/assert.hpp>
#include <string>
#include <vector>

namespace {

struct AudioCase
{
  std::string name;
  std::vector<std::uint8_t> wav;
};

void test_reads_valid_wav_variants()
{
  const auto pcm = audio_test::sample_triplet();
  const std::vector<AudioCase> cases = {
      {.name = "pcm16",            .wav = audio_test::pcm16_wav(pcm)            },
      {.name = "padded chunk",     .wav = audio_test::wav_with_padded_chunk(pcm)},
      {.name = "extensible pcm16", .wav = audio_test::extensible_pcm16_wav(pcm) },
  };

  for (const auto& audio_case : cases) {
    const auto samples = audio_test::read_samples(audio_case.wav);

    ASSERT(samples.has_value(), audio_case.name);
    ASSERT(samples->size() == 3U, audio_case.name);
    ASSERT((*samples)[0] == -1.0F, audio_case.name);
    ASSERT((*samples)[1] == 0.0F, audio_case.name);
    ASSERT((*samples)[2] == 32767.0F / 32768.0F, audio_case.name);
  }
}

void test_rejects_invalid_wav_variants()
{
  const std::vector<std::int16_t> silence = {0};
  auto wrong_riff_size = audio_test::pcm16_wav(silence);
  audio_test::write_riff_size(wrong_riff_size, 4);

  const std::vector<AudioCase> cases = {
      {.name = "wrong sample rate", .wav = audio_test::pcm16_wav_at(8000, silence)},
      {.name = "wrong riff size", .wav = wrong_riff_size},
  };

  for (const auto& audio_case : cases) {
    const auto samples = audio_test::read_samples(audio_case.wav);

    ASSERT(!samples.has_value(), audio_case.name);
  }
}

} // namespace

void run_test_audio()
{
  test_reads_valid_wav_variants();
  test_rejects_invalid_wav_variants();

  std::cout << "test_audio passed\n";
}
