#include "runtime/runtime.hpp"

#include "config/config.hpp"
#include "constants/constants.hpp"
#include "engine/engine.hpp"
#include "model/model.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"

#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/types.h>
#include <unistd.h>

namespace runtime {

namespace {

std::filesystem::path lock_dir_for(const std::filesystem::path& runtime_dir)
{
  return runtime_dir / std::string(constants::runtime::lock_dir_name);
}

bool read_pid_file(const std::filesystem::path& path, pid_t& pid)
{
  std::ifstream file(path);
  return static_cast<bool>(file >> pid);
}

bool acquire_lock(const std::filesystem::path& runtime_dir)
{
  std::filesystem::create_directories(runtime_dir);
  auto lock_dir = lock_dir_for(runtime_dir);

  std::error_code ec;
  if (std::filesystem::create_directory(lock_dir, ec)) {
    std::ofstream(lock_dir / std::string(constants::runtime::pid_file_name)) << getpid() << "\n";
    return true;
  }

  pid_t pid = 0;
  if (!read_pid_file(lock_dir / std::string(constants::runtime::pid_file_name), pid) ||
      !platform::is_process_running(pid))
  {
    platform::safe_delete_directory(lock_dir);
    if (std::filesystem::create_directory(lock_dir, ec)) {
      std::ofstream(lock_dir / std::string(constants::runtime::pid_file_name)) << getpid() << "\n";
      return true;
    }
  }

  return false;
}

void release_lock(const std::filesystem::path& runtime_dir)
{
  platform::safe_delete_directory(lock_dir_for(runtime_dir));
}

void clean_stale_payload(const std::filesystem::path& runtime_dir)
{
  platform::safe_delete_file(runtime_dir / std::string(constants::runtime::recorder_pid_file));
  platform::safe_delete_file(runtime_dir / std::string(constants::runtime::recorder_wav_file));
  platform::safe_delete_file(runtime_dir / std::string(constants::runtime::recorder_raw_file));
  platform::safe_delete_file(runtime_dir / std::string(constants::runtime::recorder_error_file));
  platform::safe_delete_file(runtime_dir / std::string(constants::runtime::state_file));
  platform::safe_delete_file(runtime_dir / std::string(constants::runtime::transcript_file));
}

std::string read_text_file(const std::filesystem::path& path)
{
  if (!std::filesystem::exists(path)) {
    return "";
  }

  std::ifstream file(path);
  if (!file.is_open()) {
    return "";
  }

  std::string output;
  std::string line;

  while (std::getline(file, line)) {
    output += line;
    output += '\n';
  }

  return output;
}

std::string read_state_file(const std::filesystem::path& runtime_dir)
{
  auto state_file = runtime_dir / std::string(constants::runtime::state_file);
  if (!std::filesystem::exists(state_file)) {
    return "";
  }

  std::ifstream file(state_file);
  std::string state;
  if (file >> state) {
    return state;
  }

  return "";
}

bool has_live_lock(const std::filesystem::path& runtime_dir)
{
  pid_t pid = 0;
  return read_pid_file(lock_dir_for(runtime_dir) / std::string(constants::runtime::pid_file_name),
                       pid) &&
         platform::is_process_running(pid);
}

bool has_live_recorder(const std::filesystem::path& runtime_dir, pid_t& pid)
{
  return read_pid_file(runtime_dir / std::string(constants::runtime::recorder_pid_file), pid) &&
         platform::is_process_running(pid);
}

std::string trim(std::string value)
{
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.pop_back();
  }

  size_t start = 0;
  while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
    ++start;
  }

  return value.substr(start);
}

void write_state(const std::filesystem::path& runtime_dir, const std::string& state)
{
  std::ofstream file(runtime_dir / std::string(constants::runtime::state_file));
  file << state << "\n";
}

void print_recorder_error(const std::filesystem::path& runtime_dir)
{
  auto error =
      trim(read_text_file(runtime_dir / std::string(constants::runtime::recorder_error_file)));
  if (!error.empty()) {
    std::cerr << error << "\n";
  }
}

void start_recording(const std::filesystem::path& runtime_dir)
{
  clean_stale_payload(runtime_dir);

  auto cfg = config::load_config();
  auto model_path = model::get_model_path(cfg.model);
  if (!std::filesystem::exists(model_path)) {
    throw std::runtime_error("model '" + cfg.model +
                             "' is not installed. Install it with: asryx --model install " +
                             cfg.model);
  }

  auto wav_path = runtime_dir / std::string(constants::runtime::recorder_wav_file);
  auto err_path = runtime_dir / std::string(constants::runtime::recorder_error_file);
  pid_t pid = engine::start_recording(wav_path.string(), err_path.string());

  std::ofstream(runtime_dir / std::string(constants::runtime::recorder_pid_file)) << pid << "\n";
  write_state(runtime_dir, std::string(constants::runtime::recording_state));
  engine::send_notification("recording…");
}

void stop_and_transcribe(const std::filesystem::path& runtime_dir, pid_t rec_pid)
{
  if (!engine::stop_recording(rec_pid)) {
    print_recorder_error(runtime_dir);
    engine::send_notification("recorder did not stop");
    return;
  }

  write_state(runtime_dir, std::string(constants::runtime::transcribing_state));

  auto cfg = config::load_config();
  auto model_path = model::get_model_path(cfg.model);
  auto wav_path = runtime_dir / std::string(constants::runtime::recorder_wav_file);
  auto output = trim(engine::transcribe(model_path, wav_path.string(), cfg.language));

  if (output.empty()) {
    engine::send_notification("no output");
    clean_stale_payload(runtime_dir);
    return;
  }

  engine::copy_to_clipboard(output);
  engine::send_notification("copied to clipboard");
  clean_stale_payload(runtime_dir);
}

} // namespace

std::string get_status()
{
  auto runtime_dir = platform::get_runtime_directory();
  auto state = read_state_file(runtime_dir);

  pid_t rec_pid = 0;
  if (has_live_recorder(runtime_dir, rec_pid)) {
    return std::string(constants::runtime::recording_state);
  }

  if (state == constants::runtime::transcribing_state && has_live_lock(runtime_dir)) {
    return std::string(constants::runtime::transcribing_state);
  }

  return std::string(constants::runtime::idle_state);
}

void toggle()
{
  auto runtime_dir = platform::get_runtime_directory();
  if (!acquire_lock(runtime_dir)) {
    return;
  }

  try {
    pid_t rec_pid = 0;
    if (has_live_recorder(runtime_dir, rec_pid)) {
      stop_and_transcribe(runtime_dir, rec_pid);
    }
    else {
      start_recording(runtime_dir);
    }

    release_lock(runtime_dir);
  }
  catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    print_recorder_error(runtime_dir);
    clean_stale_payload(runtime_dir);
    release_lock(runtime_dir);
    std::exit(1);
  }
}

} // namespace runtime
