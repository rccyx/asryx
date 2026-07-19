#ifndef ASRYX_RUNTIME_RUNTIME_HPP
#define ASRYX_RUNTIME_RUNTIME_HPP

#include <string>

namespace runtime {

std::string get_status();
void cancel();
void toggle();

#ifdef ASRYX_TESTING
namespace testing {
void reset_toggle_entry_count();
int toggle_entry_count();
} // namespace testing
#endif

} // namespace runtime

#endif // ASRYX_RUNTIME_RUNTIME_HPP
