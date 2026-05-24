#ifndef ASRYX_PLATFORM_PROCESS_HPP
#define ASRYX_PLATFORM_PROCESS_HPP

#include <string>
#include <sys/types.h>
#include <vector>

namespace platform {

bool command_exists(const std::string& name);
pid_t spawn_process_background(const std::vector<std::string>& argv,
                               const std::string& redirect_file = "");
int wait_process(pid_t pid);
bool run_process_blocking(const std::vector<std::string>& argv);
bool run_process_with_stdin(const std::vector<std::string>& argv, const std::string& input);
bool is_process_running(pid_t pid);
bool stop_process(pid_t pid, int sig = 2);

} // namespace platform

#endif // ASRYX_PLATFORM_PROCESS_HPP
