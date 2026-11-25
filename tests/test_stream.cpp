#include "engine/transcription/rolling/rolling.hpp"
#include "runtime/context.hpp"
#include "runtime/stream/stream.hpp"
#include "tests/tests.hpp"

#include <filesystem>
#include <iostream>
#include <libassert/assert.hpp>
#include <string_view>
#include <utility>

namespace {

namespace rolling = engine::transcription::rolling;
using Words = std::vector<rolling::Word>;

void _repeated_words_survive_confirmed_segment_trimming()
{
  rolling::Processor processor([](std::span<const float> audio, std::string_view) {
    const auto start = static_cast<std::uint64_t>(audio.front());
    const auto seconds = audio.size() / rolling::RATE;
    Words words;
    for (std::uint64_t t = 0; t < seconds; ++t) {
      words.push_back({t * rolling::RATE, (t + 1) * rolling::RATE, " go"});
    }
    ASSERT(start + seconds <= 65);
    return yx::ok(std::move(words));
  });
  std::vector<float> samples(65 * rolling::RATE);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const auto second = i / rolling::RATE;
    samples[i] = static_cast<float>(second);
  }
  ASSERT(processor.append(samples).has_value());
  const auto transcript = processor.finish();
  ASSERT(transcript.has_value());
  std::string expected;
  for (int i = 0; i < 65; ++i) {
    expected += " go";
  }
  ASSERT(*transcript == expected);
}

void _word_with_unstable_end_is_not_repeated()
{
  rolling::Processor processor([](std::span<const float> audio, std::string_view) {
    const auto end = audio.size() == 5 * rolling::RATE ? 3 * rolling::RATE : rolling::RATE / 2;
    return yx::ok(Words{
        {0, end, " first"}
    });
  });
  const std::vector<float> samples(11 * rolling::RATE);
  ASSERT(processor.append(samples).has_value());
  const auto transcript = processor.finish();
  ASSERT(transcript.has_value());
  ASSERT(*transcript == " first");
}

void _speech_after_long_silence_survives_window_advance()
{
  rolling::Processor processor([](std::span<const float> audio, std::string_view) {
    if (audio.back() == 0.0F) {
      return yx::ok(Words{});
    }
    return yx::ok(Words{
        {audio.size() - rolling::RATE, audio.size(), " speech after silence"}
    });
  });
  const std::vector<float> silence(30 * rolling::RATE);
  const std::vector<float> speech(rolling::RATE, 1.0F);
  ASSERT(processor.append(silence).has_value());
  ASSERT(processor.append(speech).has_value());
  const auto transcript = processor.finish();
  ASSERT(transcript.has_value());
  ASSERT(*transcript == " speech after silence");
}

void _short_recording_decodes_at_finish()
{
  rolling::Processor processor([](std::span<const float> audio, std::string_view) {
    return yx::ok(Words{
        {0, audio.size(), " short recording"}
    });
  });
  const std::vector<float> samples(2 * rolling::RATE);
  ASSERT(processor.append(samples).has_value());
  const auto transcript = processor.finish();
  ASSERT(transcript.has_value());
  ASSERT(*transcript == " short recording");
}

void _worker_failure_transcribes_original_recording()
{
  runtime_test::reset_runtime();
  runtime_test::write_recording_payload();
  runtime_test::write_text(runtime_test::runtime_file("stream.pid"),
                           std::to_string(runtime_test::dead_pid()));
  runtime_test::write_text(runtime_test::runtime_file("stream.failed"), "no confirmed boundary");
  runtime_test::toggle_runtime();
  ASSERT(runtime_test::state().stop_calls == 1);
  ASSERT(runtime_test::state().transcribe_calls == 1);
  ASSERT(runtime_test::state().copied_text == "transcript text");
  ASSERT(!runtime::stream::active(runtime_test::runtime_dir()));
  runtime_test::assert_lock_released();
}

void _completed_worker_uses_existing_delivery()
{
  runtime_test::reset_runtime();
  runtime_test::write_recording_payload();
  runtime_test::write_text(runtime_test::runtime_file("stream.pid"),
                           std::to_string(runtime_test::dead_pid()));
  runtime_test::write_text(runtime_test::runtime_file("stream.txt"), " rolling transcript ");
  runtime_test::write_text(runtime_test::runtime_file("stream.done"), "");
  runtime_test::toggle_runtime();
  ASSERT(runtime_test::state().stop_calls == 1);
  ASSERT(runtime_test::state().transcribe_calls == 0);
  ASSERT(runtime_test::state().copied_text == "rolling transcript");
  ASSERT(!runtime::stream::active(runtime_test::runtime_dir()));
  runtime_test::assert_lock_released();
}

void _cancel_rolling_recording_never_delivers()
{
  runtime_test::reset_runtime();
  runtime_test::write_recording_payload();
  runtime_test::write_text(runtime_test::runtime_file("stream.pid"),
                           std::to_string(runtime_test::dead_pid()));
  runtime_test::cancel_runtime();
  ASSERT(runtime_test::state().stop_calls == 1);
  ASSERT(runtime_test::state().clipboard_calls == 0);
  ASSERT(runtime_test::state().transcribe_calls == 0);
  ASSERT(!runtime::stream::active(runtime_test::runtime_dir()));
}

} // namespace

void run_test_stream()
{
  _repeated_words_survive_confirmed_segment_trimming();
  _word_with_unstable_end_is_not_repeated();
  _speech_after_long_silence_survives_window_advance();
  _short_recording_decodes_at_finish();
  _worker_failure_transcribes_original_recording();
  _completed_worker_uses_existing_delivery();
  _cancel_rolling_recording_never_delivers();
  runtime_test::clean_runtime();
  std::cout << "test_stream passed\n";
}
