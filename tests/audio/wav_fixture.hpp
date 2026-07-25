#ifndef ASRYX_TESTS_AUDIO_WAV_FIXTURE_HPP
#define ASRYX_TESTS_AUDIO_WAV_FIXTURE_HPP

#include "error.hpp"

#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace audio_test {

std::vector<std::int16_t> sample_triplet();
std::vector<std::uint8_t> pcm16_wav(std::span<const std::int16_t> samples);
std::vector<std::uint8_t> pcm16_wav_at(std::uint32_t sample_rate,
                                       std::span<const std::int16_t> samples);
std::vector<std::uint8_t> wav_with_padded_chunk(std::span<const std::int16_t> samples);
std::vector<std::uint8_t> extensible_pcm16_wav(std::span<const std::int16_t> samples);
void write_riff_size(std::vector<std::uint8_t>& bytes, std::uint32_t value);
std::expected<std::vector<float>, asryx::Error>
read_samples(const std::vector<std::uint8_t>& bytes);

} // namespace audio_test

#endif // ASRYX_TESTS_AUDIO_WAV_FIXTURE_HPP
