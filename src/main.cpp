#include "app/app.hpp"

#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[])
{
  std::vector<std::string> args;
  if (argc > 1) {
    args.reserve(static_cast<size_t>(argc - 1));
  }
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }
  const auto result = app::run(args);
  if (!result) {
    std::cerr << "error: " << result.error().message << "\n";
    return 1;
  }

  return *result;
}
