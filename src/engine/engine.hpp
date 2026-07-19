#ifndef ASRYX_ENGINE_ENGINE_HPP
#define ASRYX_ENGINE_ENGINE_HPP

#include <stdexcept>
#include <string>
#include <sys/types.h>

namespace engine {

class TranscriptionCancelled final : public std::runtime_error
{
public:
  TranscriptionCancelled();
};

struct TranscriptionRequest
{
  std::string model_path;
  std::string vad_model_path;
  std::string wav_path;
  std::string language;
  std::string cancel_marker_path;
};

pid_t start_recording(const std::string& wav_path, const std::string& err_path);
bool stop_recording(pid_t pid);
std::string transcribe(const TranscriptionRequest& request);
bool copy_to_clipboard(const std::string& text);
bool send_notification(const std::string& message);

#ifdef ASRYX_TESTING
namespace testing {
using StartRecordingHook = pid_t (*)(const std::string& wav_path, const std::string& err_path);
using StopRecordingHook = bool (*)(pid_t pid);
using TranscribeHook = std::string (*)(const TranscriptionRequest& request);
using CopyToClipboardHook = bool (*)(const std::string& text);
using NotificationHook = bool (*)(const std::string& message);

void set_start_recording_hook(StartRecordingHook hook);
void set_stop_recording_hook(StopRecordingHook hook);
void set_transcribe_hook(TranscribeHook hook);
void set_copy_to_clipboard_hook(CopyToClipboardHook hook);
void set_notification_hook(NotificationHook hook);
void reset_hooks();
StartRecordingHook start_recording_hook();
StopRecordingHook stop_recording_hook();
TranscribeHook transcribe_hook();
CopyToClipboardHook copy_to_clipboard_hook();
NotificationHook notification_hook();
} // namespace testing
#endif

} // namespace engine

#endif // ASRYX_ENGINE_ENGINE_HPP
