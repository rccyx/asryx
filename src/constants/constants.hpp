#ifndef ASRYX_CONSTANTS_CONSTANTS_HPP
#define ASRYX_CONSTANTS_CONSTANTS_HPP

#include <filesystem>
#include <string_view>
#include <vector>

namespace constants {

inline constexpr std::string_view app_name = "asryx";

namespace config {

inline constexpr std::string_view file_name = ".asryx.conf";
inline constexpr std::string_view temp_suffix = ".tmp";
inline constexpr std::string_view model_key = "model";
inline constexpr std::string_view language_key = "language";
inline constexpr std::string_view pipe_to_key = "pipe_to";
inline constexpr std::string_view default_model = "base.en";
inline constexpr std::string_view default_language = "auto";
inline constexpr std::string_view auto_language = "auto";
inline constexpr std::string_view english_language = "en";

} // namespace config

namespace runtime {

inline constexpr std::string_view dir_name = "asryx";
inline constexpr std::string_view fallback_tmp_root = "/tmp";
inline constexpr std::string_view lock_dir_name = "lock";
inline constexpr std::string_view pid_file_name = "pid";
inline constexpr std::string_view recorder_pid_file = "rec.pid";
inline constexpr std::string_view recorder_wav_file = "rec.wav";
inline constexpr std::string_view recorder_error_file = "rec.err";
inline constexpr std::string_view error_log_file = "error.log";
inline constexpr std::string_view cancel_marker_file = "cancel";
inline constexpr std::string_view state_file = "state";
inline constexpr std::string_view idle_state = "idle";
inline constexpr std::string_view recording_state = "recording";
inline constexpr std::string_view transcribing_state = "transcribing";

} // namespace runtime

namespace notifications {

inline constexpr std::string_view transcription_copied = "transcription copied to clipboard.";
inline constexpr std::string_view pipe_copied = "piped and copied to clipboard.";
inline constexpr std::string_view pipe_failed = "copied to clipboard (pipe failed).";
inline constexpr std::string_view cancelled = "cancelled";
inline constexpr std::string_view cancelling = "cancelling…";

} // namespace notifications

namespace paths {

inline constexpr std::string_view local_bin_dir_rel = ".local/bin";
inline constexpr std::string_view asryx_bin_rel = ".local/bin/asryx";
inline constexpr std::string_view data_dir_rel = ".local/share/asryx";
inline constexpr std::string_view models_dir_rel = ".local/share/asryx/models";
inline constexpr std::string_view whisper_checkout_rel = ".local/share/asryx/deps/whisper.cpp";
inline constexpr std::string_view whisper_pin_rel = ".local/share/asryx/versions/whisper-cpp-sha";
inline constexpr std::string_view VAD_MODEL_FILE = "ggml-silero-v6.2.0.bin";

inline std::filesystem::path config_path(const std::filesystem::path& home)
{
  return home / std::string(config::file_name);
}

inline std::filesystem::path asryx_bin_path(const std::filesystem::path& home)
{
  return home / std::string(asryx_bin_rel);
}

inline std::filesystem::path whisper_checkout_dir(const std::filesystem::path& home)
{
  return home / std::string(whisper_checkout_rel);
}

inline std::filesystem::path data_dir(const std::filesystem::path& home)
{
  return home / std::string(data_dir_rel);
}

inline std::filesystem::path models_dir(const std::filesystem::path& home)
{
  return home / std::string(models_dir_rel);
}

inline std::vector<std::filesystem::path> owned_home_paths(const std::filesystem::path& home)
{
  return {asryx_bin_path(home), data_dir(home), config_path(home)};
}

} // namespace paths

} // namespace constants

#endif // ASRYX_CONSTANTS_CONSTANTS_HPP
