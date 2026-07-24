#include "tests/model_store.hpp"

#include "constants/constants.hpp"
#include "model/model.hpp"
#include "platform/fs.hpp"

#include <filesystem>
#include <fstream>
#include <libassert/assert.hpp>
#include <string>

namespace model_store {

void write_model(const std::string& name)
{
  const auto model_path = model::get_model_path(name);
  ASSERT(model_path.has_value());
  const auto path = std::filesystem::path(*model_path);
  std::filesystem::create_directories(path.parent_path());
  std::ofstream model_file(path);
  model_file << "fake model content";
}

void write_default_model_and_vad()
{
  write_model(std::string(constants::config::default_model));
  const auto vad_model_path = model::get_vad_model_path();
  ASSERT(vad_model_path.has_value());
  std::ofstream vad_file(*vad_model_path);
  vad_file << "fake VAD model content";
}

void delete_default_model_and_vad()
{
  const auto model_path = model::get_model_path(std::string(constants::config::default_model));
  const auto vad_model_path = model::get_vad_model_path();
  ASSERT(model_path.has_value());
  ASSERT(vad_model_path.has_value());
  ASSERT(platform::safe_delete_file(*model_path).has_value());
  ASSERT(platform::safe_delete_file(*vad_model_path).has_value());
}

} // namespace model_store
