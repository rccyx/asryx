#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "platform/process.hpp"

#include <iostream>
#include <string>

namespace engine {

std::expected<bool, asryx::Error> copy_to_clipboard(const std::string& text)
{
  const auto has_wl_copy = platform::command_exists("wl-copy");
  if (!has_wl_copy) {
    return std::unexpected(has_wl_copy.error());
  }

  if (*has_wl_copy) {
    return platform::run_process_with_stdin({"wl-copy"}, text);
  }

  const auto has_xclip = platform::command_exists("xclip");
  if (!has_xclip) {
    return std::unexpected(has_xclip.error());
  }

  if (*has_xclip) {
    return platform::run_process_with_stdin({"xclip", "-selection", "clipboard"}, text);
  }

  std::cerr << "Warning: Neither wl-copy nor xclip is available to copy transcript.\n";
  return false;
}

std::expected<bool, asryx::Error> send_notification(const std::string& message)
{
  const auto has_notify_send = platform::command_exists("notify-send");
  if (!has_notify_send) {
    return std::unexpected(has_notify_send.error());
  }

  if (*has_notify_send) {
    return platform::run_process_blocking(
        {"notify-send", std::string(constants::app_name), message});
  }

  return false;
}

} // namespace engine
