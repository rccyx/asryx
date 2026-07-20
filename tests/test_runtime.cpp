#include "constants/constants.hpp"
#include "runtime/runtime.hpp"
#include "tests/test_helpers.hpp"

#include <iostream>
#include <string>

void run_test_runtime()
{
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));

  runtime::cancel();
  ASSERT_EQ(runtime::get_status(), std::string(constants::runtime::idle_state));

  std::cout << "test_runtime passed\n";
}
