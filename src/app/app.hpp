#ifndef ASRYX_APP_APP_HPP
#define ASRYX_APP_APP_HPP

#include "error.hpp"

#include <string>
#include <vector>

namespace app {

yx::Result<int> run(const std::vector<std::string>& args);

} // namespace app

#endif // ASRYX_APP_APP_HPP
