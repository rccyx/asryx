#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

void run_test_config();
void run_test_model();
void run_test_lock();
void run_test_process();
void run_test_runtime();

int main()
{
  try {
    std::filesystem::path home_path = std::filesystem::absolute("./.asryx-test-home");
    std::filesystem::path runtime_path = std::filesystem::absolute("./.asryx-test-runtime");
    std::filesystem::path bin_path = std::filesystem::absolute("./.asryx-test-bin");

    if (std::filesystem::exists(home_path)) {
      std::filesystem::remove_all(home_path);
    }
    if (std::filesystem::exists(runtime_path)) {
      std::filesystem::remove_all(runtime_path);
    }
    if (std::filesystem::exists(bin_path)) {
      std::filesystem::remove_all(bin_path);
    }

    std::filesystem::create_directories(home_path);
    std::filesystem::create_directories(runtime_path);
    std::filesystem::create_directories(bin_path);

    {
      std::ofstream recorder(bin_path / "pw-record");
      recorder << "#!/bin/sh\n"
                  "for arg do wav=\"$arg\"; done\n"
                  ": > \"$wav\"\n"
                  "trap 'exit 0' INT TERM\n"
                  "while :; do sleep 1; done\n";
    }
    {
      std::ofstream clipboard(bin_path / "wl-copy");
      clipboard << "#!/bin/sh\ncat >/dev/null\n";
    }
    {
      std::ofstream notify(bin_path / "notify-send");
      notify << "#!/bin/sh\nexit 0\n";
    }
    std::filesystem::permissions(bin_path / "pw-record", std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::add);
    std::filesystem::permissions(bin_path / "wl-copy", std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::add);
    std::filesystem::permissions(bin_path / "notify-send", std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::add);
    setenv("HOME", home_path.c_str(), 1);
    setenv("XDG_RUNTIME_DIR", runtime_path.c_str(), 1);
    const char* path_env = std::getenv("PATH");
    std::string old_path = path_env != nullptr ? path_env : "";
    std::string test_path = bin_path.string() + ":" + old_path;
    setenv("PATH", test_path.c_str(), 1);

    std::cout << "Running unit tests...\n";
    run_test_config();
    run_test_model();
    run_test_lock();
    run_test_process();
    run_test_runtime();

    std::cout << "All unit tests passed successfully!\n";

    // Clean up
    if (std::filesystem::exists(home_path)) {
      std::filesystem::remove_all(home_path);
    }
    if (std::filesystem::exists(runtime_path)) {
      std::filesystem::remove_all(runtime_path);
    }
    if (std::filesystem::exists(bin_path)) {
      std::filesystem::remove_all(bin_path);
    }
    return 0;
  }
  catch (const std::exception& e) {
    std::cerr << "Test suite failed with exception: " << e.what() << "\n";
    return 1;
  }
}
