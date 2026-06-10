#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "platform/process.hpp"

#include <iostream>
#include <string>

namespace engine {

bool copy_to_clipboard(const std::string& text)
{
#ifdef ASRYX_TESTING
  if (const auto hook = testing::copy_to_clipboard_hook()) {
    return hook(text);
  }
#endif

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
#ifdef ASRYX_TESTING
  if (const auto hook = testing::notification_hook()) {
    return hook(message);
  }
#endif

  if (platform::command_exists("notify-send")) {
    return platform::run_process_blocking(
        {"notify-send", std::string(constants::app_name), message});
  }

  return false;
}

} // namespace engine
