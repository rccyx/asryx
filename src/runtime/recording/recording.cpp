#include "runtime/recording/recording.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "model/model.hpp"
#include "platform/process.hpp"
#include "runtime/session/session.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <sys/types.h>

namespace runtime::recording {

void start(const std::filesystem::path& runtime_dir)
{
  session::clean_payload(runtime_dir);

  const auto config = config::load_config();
  model::validate_config(config);
  if (!model::is_model_installed(config.model)) {
    throw std::runtime_error("model '" + config.model +
                             "' is not installed. Install it with: asryx --model install " +
                             config.model);
  }

  model::validate_vad_model();

  const auto wav_path = session::recorder_wav_path(runtime_dir);
  const auto err_path = session::recorder_error_path(runtime_dir);
  const pid_t pid = engine::start_recording(wav_path.string(), err_path.string());
  if (!platform::is_process_running(pid)) {
    throw std::runtime_error("recorder process exited before startup completed");
  }

  std::ofstream(session::recorder_pid_path(runtime_dir)) << pid << "\n";
  session::write_state(runtime_dir, std::string(constants::runtime::recording_state));
  engine::send_notification("recording…");
}

} // namespace runtime::recording
