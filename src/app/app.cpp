#include "app/app.hpp"

#include "config/config.hpp"
#include "model/model.hpp"
#include "runtime/runtime.hpp"

#include <iostream>
#include <stdexcept>
#include <vector>

namespace app {

namespace {

void _print_usage()
{
  std::cerr << "asryx - native Linux voice to text toggle\n\n"
            << "Usage:\n"
            << "  asryx                           Toggle recording/transcription\n"
            << "  asryx status                    Print runtime state\n"
            << "  asryx --pipe-to <command>       Set post-copy pipe command\n"
            << "  asryx --no-pipe                 Clear post-copy pipe command\n"
            << "  asryx --language <code|auto>    Select transcription language\n"
            << "  asryx --model list              List available model sizes\n"
            << "  asryx --model install <name>    Install a model\n"
            << "  asryx --model use <name>        Select active model\n"
            << "  asryx --model uninstall <name>  Uninstall a model\n";
}

void _set_pipe_to(const std::string& command)
{
  if (command.empty()) {
    throw std::runtime_error("--pipe-to requires a non-empty command string");
  }

  auto config = config::load_config();
  config.pipe_to = command;
  config::save_config(config);
}

void _clear_pipe_to()
{
  auto config = config::load_config();
  config.pipe_to.clear();
  config::save_config(config);
}

} // namespace

int run(const std::vector<std::string>& args)
{
  try {
    if (args.empty()) {
      runtime::toggle();
      return 0;
    }

    if (args.size() == 1 && args[0] == "status") {
      std::cout << runtime::get_status() << "\n";
      return 0;
    }

    if (args.size() == 2 && args[0] == "--pipe-to") {
      _set_pipe_to(args[1]);
      return 0;
    }

    if (args.size() == 1 && args[0] == "--no-pipe") {
      _clear_pipe_to();
      return 0;
    }

    if (args.size() == 2 && args[0] == "--model") {
      if (args[1] == "list") {
        model::list_models();
        return 0;
      }
    }

    if (args.size() == 2 && args[0] == "--language") {
      model::use_language(args[1]);
      return 0;
    }

    if (args.size() == 3 && args[0] == "--model") {
      if (args[1] == "install") {
        model::install_model(args[2]);
        return 0;
      }

      if (args[1] == "use") {
        model::use_model(args[2]);
        return 0;
      }

      if (args[1] == "uninstall") {
        model::uninstall_model(args[2]);
        return 0;
      }
    }

    std::cerr << "error: invalid arguments\n\n";
    _print_usage();
    return 1;
  }
  catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}

} // namespace app
