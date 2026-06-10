#include "engine/engine.hpp"

#ifdef ASRYX_TESTING

namespace engine::testing {

namespace {

struct Hooks
{
  StartRecordingHook start_recording = nullptr;
  StopRecordingHook stop_recording = nullptr;
  TranscribeHook transcribe = nullptr;
  CopyToClipboardHook copy_to_clipboard = nullptr;
  NotificationHook notification = nullptr;
};

Hooks& _hooks()
{
  static Hooks value;
  return value;
}

} // namespace

void set_start_recording_hook(StartRecordingHook hook)
{
  _hooks().start_recording = hook;
}

void set_stop_recording_hook(StopRecordingHook hook)
{
  _hooks().stop_recording = hook;
}

void set_transcribe_hook(TranscribeHook hook)
{
  _hooks().transcribe = hook;
}

void set_copy_to_clipboard_hook(CopyToClipboardHook hook)
{
  _hooks().copy_to_clipboard = hook;
}

void set_notification_hook(NotificationHook hook)
{
  _hooks().notification = hook;
}

void reset_hooks()
{
  _hooks() = Hooks{};
}

StartRecordingHook start_recording_hook()
{
  return _hooks().start_recording;
}

StopRecordingHook stop_recording_hook()
{
  return _hooks().stop_recording;
}

TranscribeHook transcribe_hook()
{
  return _hooks().transcribe;
}

CopyToClipboardHook copy_to_clipboard_hook()
{
  return _hooks().copy_to_clipboard;
}

NotificationHook notification_hook()
{
  return _hooks().notification;
}

} // namespace engine::testing

#endif
