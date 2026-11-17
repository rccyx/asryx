#ifndef ASRYX_RUNTIME_TRANSCRIPTION_TRANSCRIPTION_HPP
#define ASRYX_RUNTIME_TRANSCRIPTION_TRANSCRIPTION_HPP

#include "config/config.hpp"
#include "error.hpp"

#include <filesystem>
#include <string>
#include <sys/types.h>

namespace runtime::transcription {

struct DeliveryInput
{
  std::filesystem::path runtime_dir;
  config::Config config;
  std::string output;
};

yx::Result<void> deliver(const DeliveryInput& input);
yx::Result<void> transcribe_stopped(const std::filesystem::path& runtime_dir);
yx::Result<void> stop_and_transcribe(const std::filesystem::path& runtime_dir, pid_t rec_pid);

} // namespace runtime::transcription

#endif // ASRYX_RUNTIME_TRANSCRIPTION_TRANSCRIPTION_HPP
