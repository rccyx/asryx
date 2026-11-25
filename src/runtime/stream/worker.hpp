#ifndef ASRYX_RUNTIME_STREAM_WORKER_HPP
#define ASRYX_RUNTIME_STREAM_WORKER_HPP
#include "error.hpp"

#include <filesystem>
#include <string>

namespace runtime::stream::worker {
yx::Result<std::string> compute(const std::filesystem::path& dir);
} // namespace runtime::stream::worker
#endif
