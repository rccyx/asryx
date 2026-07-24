#ifndef ASRYX_RUNTIME_RUNTIME_HPP
#define ASRYX_RUNTIME_RUNTIME_HPP

#include "error.hpp"

#include <expected>
#include <string>

namespace runtime {

std::expected<std::string, asryx::Error> get_status();
std::expected<void, asryx::Error> cancel();
std::expected<void, asryx::Error> toggle();

} // namespace runtime

#endif // ASRYX_RUNTIME_RUNTIME_HPP
