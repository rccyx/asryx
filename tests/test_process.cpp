#include "platform/process.hpp"
#include "tests/tests.hpp"

#include <cstddef>
#include <iostream>
#include <libassert/assert.hpp>
#include <string>
#include <unistd.h>

void run_test_process()
{
  const auto sh_exists = platform::command_exists("sh");
  ASSERT(sh_exists.has_value());
  ASSERT(*sh_exists);

  const auto exits_zero = platform::run_process_blocking({"sh", "-c", "exit 0"});
  ASSERT(exits_zero.has_value());
  ASSERT(*exits_zero);

  const auto exits_nonzero = platform::run_process_blocking({"sh", "-c", "exit 7"});
  ASSERT(exits_nonzero.has_value());
  ASSERT(!*exits_nonzero);

  const auto accepts_stdin =
      platform::run_process_with_stdin({"sh", "-c", "cat >/dev/null"}, "hello");
  ASSERT(accepts_stdin.has_value());
  ASSERT(*accepts_stdin);

  const auto stdin_nonzero = platform::run_process_with_stdin({"sh", "-c", "exit 7"}, "hello");
  ASSERT(stdin_nonzero.has_value());
  ASSERT(!*stdin_nonzero);

  constexpr size_t UNREAD_INPUT_SIZE = 1024ULL * 1024ULL;
  const std::string unread_input(UNREAD_INPUT_SIZE, 'x');
  const auto stdin_closed =
      platform::run_process_with_stdin({"sh", "-c", "head -c 1 >/dev/null"}, unread_input);
  ASSERT(!stdin_closed.has_value());
  ASSERT(platform::is_process_running(getpid()));

  std::cout << "test_process passed\n";
}
