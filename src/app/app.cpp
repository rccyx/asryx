#include "app/app.hpp"

#include "config/config.hpp"
#include "engine/engine.hpp"
#include "model/model.hpp"
#include "runtime/runtime.hpp"

#include <iostream>
#include <vector>

namespace app {

namespace {

void _print_usage()
{
  std::cerr << "asryx - native Linux voice to text toggle\n\n"
            << "Usage:\n"
            << "  asryx                           Toggle recording/transcription\n"
            << "  asryx cancel                    Cancel recording/transcription\n"
            << "  asryx status                    Print runtime state\n"
            << "  asryx --pipe-to <command>       Set post-copy pipe command\n"
            << "  asryx --no-pipe                 Clear post-copy pipe command\n"
            << "  asryx --language <code|auto>    Select transcription language\n"
            << "  asryx --prompt <text>           Set Whisper prompt\n"
            << "  asryx --no-prompt               Clear Whisper prompt\n"
            << "  asryx --model list              List available model sizes\n"
            << "  asryx --model install <name>    Install a model\n"
            << "  asryx --model quantize <name> <quant>\n"
            << "  asryx --model use <name>        Select active model\n"
            << "  asryx --model uninstall <name>  Uninstall a model\n";
}

yx::Result<void> _set_pipe_to(const std::string& command)
{
  if (command.empty()) {
    return yx::fail("--pipe-to requires a non-empty command string");
  }

  return config::load_config().and_then([&command](config::Config config) {
    config.pipe_to = command;
    return config::save_config(config);
  });
}

yx::Result<void> _clear_pipe_to()
{
  return config::load_config().and_then([](config::Config config) {
    config.pipe_to.clear();
    return config::save_config(config);
  });
}

yx::Result<void> _set_prompt(const std::string& prompt)
{
  if (prompt.empty()) {
    return yx::fail("--prompt requires a non-empty prompt string");
  }

  return config::load_config().and_then([&prompt](config::Config config) {
    return model::get_model_path(config.model)
        .and_then([&prompt](const std::string& model_path) {
          return engine::validate_prompt(model_path, prompt);
        })
        .and_then([&config, &prompt] {
          config.prompt = prompt;
          return config::save_config(config);
        });
  });
}

yx::Result<void> _clear_prompt()
{
  return config::load_config().and_then([](config::Config config) {
    config.prompt.clear();
    return config::save_config(config);
  });
}

yx::Result<int> _headless_exit(const yx::Result<void>& result)
{
  return yx::ok(result ? 0 : 1);
}

} // namespace

yx::Result<int> run(const std::vector<std::string>& args)
{
  if (args.empty()) {
    return _headless_exit(runtime::toggle());
  }

  if (args.size() == 1 && args[0] == "cancel") {
    return _headless_exit(runtime::cancel());
  }

  if (args.size() == 1 && args[0] == "status") {
    return runtime::get_status().transform([](const std::string& status) {
      std::cout << status << "\n";
      return 0;
    });
  }

  if (args.size() == 2 && args[0] == "--pipe-to") {
    return _set_pipe_to(args[1]).transform([] { return 0; });
  }

  if (args.size() == 1 && args[0] == "--no-pipe") {
    return _clear_pipe_to().transform([] { return 0; });
  }

  if (args.size() == 2 && args[0] == "--prompt") {
    return _set_prompt(args[1]).transform([] { return 0; });
  }

  if (args.size() == 1 && args[0] == "--no-prompt") {
    return _clear_prompt().transform([] { return 0; });
  }

  if (args.size() == 2 && args[0] == "--model") {
    if (args[1] == "list") {
      return model::list_models().transform([] { return 0; });
    }
  }

  if (args.size() == 2 && args[0] == "--language") {
    return model::use_language(args[1]).transform([] { return 0; });
  }

  if (args.size() == 3 && args[0] == "--model") {
    if (args[1] == "install") {
      return model::install_model(args[2]).transform([] { return 0; });
    }

    if (args[1] == "use") {
      return model::use_model(args[2]).transform([] { return 0; });
    }

    if (args[1] == "uninstall") {
      return model::uninstall_model(args[2]).transform([] { return 0; });
    }
  }

  if (args.size() == 4 && args[0] == "--model" && args[1] == "quantize") {
    return model::quantize_model(args[2], args[3]).transform([] { return 0; });
  }

  std::cerr << "error: invalid arguments\n\n";
  _print_usage();
  return yx::ok(1);
}

} // namespace app
