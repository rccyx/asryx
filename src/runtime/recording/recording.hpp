#ifndef ASRYX_RUNTIME_RECORDING_RECORDING_HPP
#define ASRYX_RUNTIME_RECORDING_RECORDING_HPP

#include "error.hpp"

#include <expected>
#include <filesystem>

namespace runtime::recording {

std::expected<void, asryx::Error> start(const std::filesystem::path& runtime_dir);

} // namespace runtime::recording

#endif // ASRYX_RUNTIME_RECORDING_RECORDING_HPP
