#ifndef ASRYX_PLATFORM_PROCESS_EXEC_HPP
#define ASRYX_PLATFORM_PROCESS_EXEC_HPP

#include "error.hpp"

#include <string>
#include <vector>

namespace platform::process_detail {

std::vector<char*> build_argv(const std::vector<std::string>& argv);
void redirect_stdout_to_devnull();
void redirect_stderr_to_file(const std::string& path);
yx::Result<void> close_fd(int fd);
yx::Result<void> write_all(int fd, const std::string& input);

} // namespace platform::process_detail

#endif // ASRYX_PLATFORM_PROCESS_EXEC_HPP
