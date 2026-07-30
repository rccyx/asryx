#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "platform/process.hpp"

#include <string>

namespace engine {

yx::Result<bool> copy_to_clipboard(const std::string& text)
{
  const auto has_wl_copy = platform::command_exists("wl-copy");
  if (!has_wl_copy) {
    return yx::fail(has_wl_copy.error());
  }

  if (*has_wl_copy) {
    return platform::run_process_with_stdin({"wl-copy"}, text);
  }

  const auto has_xclip = platform::command_exists("xclip");
  if (!has_xclip) {
    return yx::fail(has_xclip.error());
  }

  if (*has_xclip) {
    return platform::run_process_with_stdin({"xclip", "-selection", "clipboard"}, text);
  }

  return yx::ok(false);
}

yx::Result<bool> send_notification(const std::string& message)
{
  const auto has_notify_send = platform::command_exists("notify-send");
  if (!has_notify_send) {
    return yx::fail(has_notify_send.error());
  }

  if (*has_notify_send) {
    return platform::run_process_blocking(
        {"notify-send", std::string(constants::app_name), message});
  }

  return yx::ok(false);
}

} // namespace engine
