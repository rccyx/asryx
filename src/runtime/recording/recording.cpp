#include "runtime/recording/recording.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "model/model.hpp"
#include "platform/process.hpp"
#include "runtime/session/session.hpp"

#include <filesystem>
#include <string>
#include <sys/types.h>

namespace runtime::recording {

yx::Result<void> start(const std::filesystem::path& runtime_dir)
{
  const auto config =
      session::clean_payload(runtime_dir).and_then([] { return config::load_config(); });
  if (!config) {
    return yx::fail(config.error());
  }

  const auto installed = model::validate_config(*config).and_then(
      [&config] { return model::is_model_installed(config->model); });
  if (!installed) {
    return yx::fail(installed.error());
  }

  if (!*installed) {
    return yx::fail("model '" + config->model +
                    "' is not installed. Install it with: asryx --model install " + config->model);
  }

  const auto wav_path = session::recorder_wav_path(runtime_dir);
  const auto err_path = session::recorder_error_path(runtime_dir);
  return model::validate_vad_model()
      .and_then([&wav_path, &err_path] {
        return engine::start_recording(wav_path.string(), err_path.string());
      })
      .and_then([&runtime_dir](pid_t pid) -> yx::Result<void> {
        if (!platform::is_process_running(pid)) {
          return yx::fail("recorder process exited before startup completed");
        }

        const auto pid_written = session::write_recorder_pid(runtime_dir, pid);
        if (!pid_written) {
          return yx::fail(pid_written.error());
        }

        const auto state_written =
            session::write_state(runtime_dir, std::string(constants::runtime::recording_state));
        if (!state_written) {
          return yx::fail(state_written.error());
        }

        return engine::send_notification("recording...").transform([](bool) {});
      });
}

} // namespace runtime::recording
