#include "engine/engine.hpp"

#include "engine/recorder/recorder.hpp"
#include "engine/transcription/transcription.hpp"

#include <string>

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

} // namespace

TranscriptionCancelled::TranscriptionCancelled()
    : std::runtime_error("transcription canceled")
{
}

pid_t start_recording(const std::string& wav_path, const std::string& err_path)
{
  ASRYX_TEST_HOOK(start_recording_hook, wav_path, err_path);
  return recorder::start(wav_path, err_path);
}

bool stop_recording(pid_t pid)
{
  ASRYX_TEST_HOOK(stop_recording_hook, pid);
  return recorder::stop(pid);
}

std::string transcribe(const TranscriptionRequest& request)
{
  ASRYX_TEST_HOOK(transcribe_hook, request);
  return transcription::run(request);
}

} // namespace engine
