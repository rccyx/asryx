#ifndef ASRYX_RUNTIME_RECORDING_RECORDING_HPP
#define ASRYX_RUNTIME_RECORDING_RECORDING_HPP

#include <filesystem>

namespace runtime::recording {

void start(const std::filesystem::path& runtime_dir);

} // namespace runtime::recording

#endif // ASRYX_RUNTIME_RECORDING_RECORDING_HPP
