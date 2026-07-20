#include "runtime/transcription/transcription.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "model/model.hpp"
#include "platform/process.hpp"
#include "runtime/session/session.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <sys/types.h>

namespace runtime::transcription {

namespace {

void _route(const std::filesystem::path& runtime_dir, const config::Config& config,
            const std::string& output)
{
  if (!engine::copy_to_clipboard(output)) {
    const auto log_path =
        session::write_log(runtime_dir, "clipboard copy failed! transcript was not copied.\n");
    std::cerr << "clipboard failed! see log: " << log_path << "\n";
    engine::send_notification("clipboard failed! see log");
    session::clean_payload(runtime_dir);
    return;
  }

  if (config.pipe_to.empty()) {
    engine::send_notification(std::string(constants::notifications::transcription_copied));
    session::clean_payload(runtime_dir);
    return;
  }

  if (!platform::run_process_with_stdin({"sh", "-c", config.pipe_to}, output)) {
    const auto log_path = session::write_log(
        runtime_dir, "pipe target failed! transcript was copied to clipboard.\n");
    std::cerr << "pipe target failed! transcript remains in clipboard, see log: " << log_path
              << "\n";
    engine::send_notification(std::string(constants::notifications::pipe_failed));
    session::clean_payload(runtime_dir);
    return;
  }

  engine::send_notification(std::string(constants::notifications::pipe_copied));
  session::clean_payload(runtime_dir);
}

} // namespace

void stop_and_transcribe(const std::filesystem::path& runtime_dir, pid_t rec_pid)
{
  if (!engine::stop_recording(rec_pid)) {
    session::print_recorder_error(runtime_dir);
    engine::send_notification("recorder didn't stop");
    return;
  }

  session::write_state(runtime_dir, std::string(constants::runtime::transcribing_state));

  const auto config = config::load_config();
  const auto language = model::transcription_language_for(config);
  const engine::TranscriptionRequest request{
      .model_path = model::get_model_path(config.model),
      .vad_model_path = model::get_vad_model_path(),
      .wav_path = session::recorder_wav_path(runtime_dir).string(),
      .language = language,
      .cancel_marker_path = session::cancel_marker_path(runtime_dir).string()};
  std::string output;
  try {
    output = session::trim(engine::transcribe(request));
  }
  catch (const engine::TranscriptionCancelled&) {
    session::clean_payload(runtime_dir);
    engine::send_notification(std::string(constants::notifications::cancelled));
    return;
  }
  catch (const std::exception&) {
    if (session::cancel_requested(runtime_dir)) {
      session::clean_payload(runtime_dir);
      engine::send_notification(std::string(constants::notifications::cancelled));
      return;
    }

    throw;
  }

  if (session::cancel_requested(runtime_dir)) {
    session::clean_payload(runtime_dir);
    engine::send_notification(std::string(constants::notifications::cancelled));
    return;
  }

  if (output.empty()) {
    engine::send_notification("no output");
    session::clean_payload(runtime_dir);
    return;
  }

  _route(runtime_dir, config, output);
}

} // namespace runtime::transcription
