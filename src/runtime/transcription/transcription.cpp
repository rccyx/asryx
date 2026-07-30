#include "runtime/transcription/transcription.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "model/model.hpp"
#include "platform/process.hpp"
#include "runtime/session/session.hpp"

#include <filesystem>
#include <string>
#include <sys/types.h>
#include <utility>

namespace runtime::transcription {

namespace {

struct TranscriptionContext
{
  config::Config config;
  engine::TranscriptionRequest request;
};

yx::Result<void> _clean_cancelled(const std::filesystem::path& runtime_dir)
{
  return session::clean_payload(runtime_dir).transform([] {
    yx::ignore_failure(engine::send_notification(std::string(constants::notifications::cancelled)));
  });
}

yx::Result<TranscriptionContext> _build_context(const std::filesystem::path& runtime_dir,
                                                config::Config config)
{
  return model::transcription_language_for(config).and_then(
      [&runtime_dir, config = std::move(config)](const std::string& language) {
        return model::get_model_path(config.model)
            .and_then([&runtime_dir, config, language](const std::string& model_path) {
              return model::get_vad_model_path().transform(
                  [&runtime_dir, config, language, model_path](std::string vad_model_path) {
                    return TranscriptionContext{
                        .config = config,
                        .request = {.model_path = model_path,
                                    .vad_model_path = std::move(vad_model_path),
                                    .wav_path = session::recorder_wav_path(runtime_dir).string(),
                                    .language = language,
                                    .cancel_marker_path =
                                        session::cancel_marker_path(runtime_dir).string()}
                    };
                  });
            });
      });
}

yx::Result<void> _route(const std::filesystem::path& runtime_dir, const config::Config& config,
                        const std::string& output)
{
  const auto copied = engine::copy_to_clipboard(output);
  if (!copied) {
    return yx::fail(copied.error());
  }

  if (!*copied) {
    yx::ignore_failure(
        session::write_log(runtime_dir, "clipboard copy failed! transcript was not copied.\n"));
    yx::ignore_failure(
        engine::send_notification(std::string(constants::notifications::clipboard_failed)));
    return session::clean_payload(runtime_dir);
  }

  if (config.pipe_to.empty()) {
    yx::ignore_failure(
        engine::send_notification(std::string(constants::notifications::transcription_copied)));
    return session::clean_payload(runtime_dir);
  }

  const auto piped = platform::run_process_with_stdin({"sh", "-c", config.pipe_to}, output);
  if (!piped) {
    return yx::fail(piped.error());
  }

  if (!*piped) {
    yx::ignore_failure(session::write_log(
        runtime_dir, "pipe target failed! transcript was copied to clipboard.\n"));
    yx::ignore_failure(
        engine::send_notification(std::string(constants::notifications::pipe_failed)));
    return session::clean_payload(runtime_dir);
  }

  yx::ignore_failure(engine::send_notification(std::string(constants::notifications::pipe_copied)));
  return session::clean_payload(runtime_dir);
}

} // namespace

yx::Result<void> stop_and_transcribe(const std::filesystem::path& runtime_dir, pid_t rec_pid)
{
  const auto stopped = engine::stop_recording(rec_pid);
  if (!stopped) {
    return yx::fail(stopped.error());
  }

  if (!*stopped) {
    yx::ignore_failure(engine::send_notification("recorder didn't stop"));
    return yx::ok();
  }

  const auto state_written =
      session::write_state(runtime_dir, std::string(constants::runtime::transcribing_state));
  if (!state_written) {
    return yx::fail(state_written.error());
  }

  const auto context = config::load_config().and_then([&runtime_dir](config::Config config) {
    return _build_context(runtime_dir, std::move(config));
  });
  if (!context) {
    return yx::fail(context.error());
  }

  const auto transcription = engine::transcribe(context->request);
  if (!transcription) {
    if (session::cancel_requested(runtime_dir)) {
      return _clean_cancelled(runtime_dir);
    }

    return yx::fail(transcription.error());
  }

  if (session::cancel_requested(runtime_dir)) {
    return _clean_cancelled(runtime_dir);
  }

  const auto output = session::trim(*transcription);
  if (output.empty()) {
    yx::ignore_failure(engine::send_notification("no output"));
    return session::clean_payload(runtime_dir);
  }

  return _route(runtime_dir, context->config, output);
}

} // namespace runtime::transcription
