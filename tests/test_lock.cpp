#include "constants/constants.hpp"
#include "runtime/runtime.hpp"
#include "tests/test_helpers.hpp"

#include <iostream>

void run_test_lock()
{
  std::string status = runtime::get_status();
  ASSERT_EQ(status, std::string(constants::runtime::idle_state));

  std::cout << "test_lock passed\n";
}
