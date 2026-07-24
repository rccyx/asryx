#ifndef ASRYX_PLATFORM_PROCESS_HPP
#define ASRYX_PLATFORM_PROCESS_HPP

#include "error.hpp"

#include <expected>
#include <string>
#include <sys/types.h>
#include <vector>

namespace platform {

std::expected<bool, asryx::Error> command_exists(const std::string& name);
std::expected<pid_t, asryx::Error> spawn_process_background(
    const std::vector<std::string>& argv,
    const std::string& redirect_file = "");
std::expected<int, asryx::Error> wait_process(pid_t pid);
std::expected<bool, asryx::Error> run_process_blocking(const std::vector<std::string>& argv);
std::expected<bool, asryx::Error> run_process_with_stdin(const std::vector<std::string>& argv,
                                                         const std::string& input);
bool is_process_running(pid_t pid);
std::expected<bool, asryx::Error> stop_process(pid_t pid, int sig = 2);

} // namespace platform

#endif // ASRYX_PLATFORM_PROCESS_HPP
