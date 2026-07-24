#ifndef ASRYX_RUNTIME_SESSION_SESSION_HPP
#define ASRYX_RUNTIME_SESSION_SESSION_HPP

#include "error.hpp"

#include <expected>
#include <filesystem>
#include <optional>
#include <string>
#include <sys/types.h>

namespace runtime::session {

std::filesystem::path cancel_marker_path(const std::filesystem::path& runtime_dir);
std::filesystem::path recorder_wav_path(const std::filesystem::path& runtime_dir);
std::filesystem::path recorder_error_path(const std::filesystem::path& runtime_dir);
std::filesystem::path recorder_pid_path(const std::filesystem::path& runtime_dir);

std::expected<bool, asryx::Error> acquire_lock(const std::filesystem::path& runtime_dir);
std::expected<void, asryx::Error> release_lock(const std::filesystem::path& runtime_dir);
std::optional<pid_t> live_recorder_pid(const std::filesystem::path& runtime_dir);

std::expected<void, asryx::Error> clean_payload(const std::filesystem::path& runtime_dir);
void write_state(const std::filesystem::path& runtime_dir, const std::string& state);
std::string status_for(const std::filesystem::path& runtime_dir);
bool wait_for_idle(const std::filesystem::path& runtime_dir);

std::expected<bool, asryx::Error> create_cancel_marker(const std::filesystem::path& runtime_dir);
bool cancel_requested(const std::filesystem::path& runtime_dir);

std::string trim(std::string value);
std::string recorder_error_text(const std::filesystem::path& runtime_dir);
void print_recorder_error(const std::filesystem::path& runtime_dir);
std::filesystem::path write_log(const std::filesystem::path& runtime_dir,
                                const std::string& content);

} // namespace runtime::session

#endif // ASRYX_RUNTIME_SESSION_SESSION_HPP
