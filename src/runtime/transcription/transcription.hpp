#ifndef ASRYX_RUNTIME_TRANSCRIPTION_TRANSCRIPTION_HPP
#define ASRYX_RUNTIME_TRANSCRIPTION_TRANSCRIPTION_HPP

#include "error.hpp"

#include <filesystem>
#include <sys/types.h>

namespace runtime::transcription {

yx::Result<void> stop_and_transcribe(const std::filesystem::path& runtime_dir, pid_t rec_pid);

} // namespace runtime::transcription

#endif // ASRYX_RUNTIME_TRANSCRIPTION_TRANSCRIPTION_HPP
