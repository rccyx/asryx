#include "engine/transcription/rolling/rolling.hpp"
#include "runtime/context.hpp"
#include "runtime/stream/stream.hpp"
#include "tests/tests.hpp"

#include <filesystem>
#include <iostream>
#include <libassert/assert.hpp>
#include <utility>

namespace {

namespace rolling = engine::transcription::rolling;

void _repeated_words_survive_confirmed_segment_trimming()
{
  rolling::Processor processor(
      [](std::span<const float> audio) -> yx::Result<std::vector<rolling::Segment>> {
        const auto start = static_cast<std::uint64_t>(audio.front());
        const auto seconds = audio.size() / rolling::RATE;
        std::vector<rolling::Segment> segments;
        for (std::uint64_t t = 0; t < seconds; ++t) {
          segments.push_back({t * rolling::RATE, (t + 1) * rolling::RATE, " go"});
        }
        ASSERT(start + seconds <= 65);
        return yx::ok(std::move(segments));
      });
  std::vector<float> samples(65 * rolling::RATE);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] = static_cast<float>(i / rolling::RATE);
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

yx::Result<std::vector<rolling::Segment>> _continuous_speech(std::span<const float> audio)
{
  return yx::ok(std::vector<rolling::Segment>{
      {0, audio.size(), " continuous speech"}
  });
}

void _unconfirmed_window_requires_offline_fallback()
{
  rolling::Processor processor(_continuous_speech);
  const std::vector<float> samples(30 * rolling::RATE);
  const auto appended = processor.append(samples);
  ASSERT(!appended.has_value());
  ASSERT(appended.error().message.find("no confirmed boundary") != std::string::npos);
}

void _short_recording_decodes_at_finish()
{
  rolling::Processor processor(_continuous_speech);
  const std::vector<float> samples(2 * rolling::RATE);
  ASSERT(processor.append(samples).has_value());
  const auto transcript = processor.finish();
  ASSERT(transcript.has_value());
  ASSERT(*transcript == " continuous speech");
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
  _unconfirmed_window_requires_offline_fallback();
  _short_recording_decodes_at_finish();
  _worker_failure_transcribes_original_recording();
  _completed_worker_uses_existing_delivery();
  _cancel_rolling_recording_never_delivers();
  runtime_test::clean_runtime();
  std::cout << "test_stream passed\n";
}
