#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "platform/process.hpp"

#include <cstdlib>
#include <string>

namespace engine {

yx::Result<bool> copy_to_clipboard(const std::string& text)
{
  const char* env_session = std::getenv("XDG_SESSION_TYPE");

  if (!env_session) {
    return yx::fail(std::string("Unsupported session type: XDG_SESSION_TYPE is not set"));
  }

  const std::string session_type(env_session);

  if (session_type == "wayland") {
    const auto has_wl_copy = platform::command_exists("wl-copy");
    if (!has_wl_copy) {
      return yx::fail(has_wl_copy.error());
    }

    if (*has_wl_copy) {
      return platform::run_process_with_stdin({"wl-copy"}, text);
    }
    return yx::ok(false);
  }

  if (session_type == "x11") {
    const auto has_xclip = platform::command_exists("xclip");
    if (!has_xclip) {
      return yx::fail(has_xclip.error());
    }

    if (*has_xclip) {
      return platform::run_process_with_stdin({"xclip", "-selection", "clipboard"}, text);
    }
    return yx::ok(false);
  }

  return yx::fail(std::string("Unsupported session type: ") + session_type);
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