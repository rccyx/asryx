#ifndef ASRYX_RUNTIME_STREAM_HPP
#define ASRYX_RUNTIME_STREAM_HPP
#include "error.hpp"

#include <filesystem>
#include <sys/types.h>

namespace runtime::stream {
bool active(const std::filesystem::path& dir);
yx::Result<void> start(const std::filesystem::path& dir);
yx::Result<void> stop_and_deliver(const std::filesystem::path& dir, pid_t recorder);
yx::Result<void> cancel_worker(const std::filesystem::path& dir);
yx::Result<void> clean(const std::filesystem::path& dir);
yx::Result<int> run_worker();
} // namespace runtime::stream
#endif
