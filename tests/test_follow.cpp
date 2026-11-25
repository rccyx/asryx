#include "audio/wav_fixture.hpp"
#include "engine/audio/wav/follow.hpp"
#include "platform/fs.hpp"
#include "runtime/context.hpp"
#include "tests/tests.hpp"

#include <fstream>
#include <iostream>
#include <libassert/assert.hpp>
#include <span>

namespace {

void _growing_wav_preserves_partial_samples()
{
  const auto path = runtime_test::runtime_file("follow.wav");
  const auto samples = audio_test::sample_triplet();
  auto bytes = audio_test::pcm16_wav(samples);
  audio_test::write_riff_size(bytes, 0xffffffffU);
  const auto header_size = bytes.size() - (samples.size() * 2);
  const std::span<const std::uint8_t> wav(bytes);
  {
    std::ofstream out(path, std::ios::binary);
    audio_test::write_bytes(out, wav.first(header_size));
  }
  auto follower = engine::audio::WavFollower::open(path);
  ASSERT(follower.has_value());
  {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    audio_test::write_bytes(out, wav.subspan(header_size, 3));
  }
  const auto first = follower->read();
  ASSERT(first.has_value());
  ASSERT(first->size() == 1);
  ASSERT((*first)[0] == static_cast<float>(samples[0]) / 32768.0F);
  {
    std::ofstream out(path, std::ios::binary | std::ios::app);
    audio_test::write_bytes(out, wav.subspan(header_size + 3, 1));
  }
  const auto second = follower->read();
  ASSERT(second.has_value());
  ASSERT(second->size() == 1);
  ASSERT((*second)[0] == static_cast<float>(samples[1]) / 32768.0F);
  ASSERT(follower->read()->empty());
  ASSERT(platform::safe_delete_file(path).has_value());
}

} // namespace

void run_test_follow()
{
  _growing_wav_preserves_partial_samples();
  std::cout << "test_follow passed\n";
}
