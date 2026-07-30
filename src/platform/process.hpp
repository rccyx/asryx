#ifndef ASRYX_PLATFORM_PROCESS_HPP
#define ASRYX_PLATFORM_PROCESS_HPP

#include "error.hpp"

#include <string>
#include <sys/types.h>
#include <vector>

namespace platform {

yx::Result<bool> command_exists(const std::string& name);
yx::Result<pid_t> spawn_process_background(const std::vector<std::string>& argv,
                                           const std::string& redirect_file = "");
yx::Result<int> wait_process(pid_t pid);
yx::Result<bool> run_process_blocking(const std::vector<std::string>& argv);
yx::Result<bool> run_process_with_stdin(const std::vector<std::string>& argv,
                                        const std::string& input);
bool is_process_running(pid_t pid);
yx::Result<bool> stop_process(pid_t pid, int sig = 2);

} // namespace platform

#endif // ASRYX_PLATFORM_PROCESS_HPP
