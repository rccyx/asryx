#ifndef ASRYX_RUNTIME_RUNTIME_HPP
#define ASRYX_RUNTIME_RUNTIME_HPP

#include "error.hpp"

#include <string>

namespace runtime {

yx::Result<std::string> get_status();
yx::Result<void> cancel();
yx::Result<void> toggle();

} // namespace runtime

#endif // ASRYX_RUNTIME_RUNTIME_HPP
