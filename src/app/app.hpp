#ifndef ASRYX_APP_APP_HPP
#define ASRYX_APP_APP_HPP

#include "error.hpp"

#include <expected>
#include <string>
#include <vector>

namespace app {

std::expected<int, asryx::Error> run(const std::vector<std::string>& args);

} // namespace app

#endif // ASRYX_APP_APP_HPP
