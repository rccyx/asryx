#include "model/model.hpp"
#include "model/quantization/artifact.hpp"
#include "model/store.hpp"
#include "platform/fs.hpp"
#include "platform/process.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <vector>

namespace model {

namespace {

std::string _supported_quant_types_message()
{
  std::string message;

  for (const auto& quant : get_supported_quant_types()) {
    if (!message.empty()) {
      message += ", ";
    }

    message += quant;
  }

  return message;
}

yx::Result<std::filesystem::path> _quantized_model_path(const artifact::ModelArtifact& artifact)
{
  return get_model_path(artifact::quantized_model_name(artifact))
      .transform([](const std::string& path) { return std::filesystem::path(path); });
}

std::filesystem::path _temporary_model_path(const std::filesystem::path& target_path)
{
  return target_path.parent_path() / ("." + target_path.filename().string() + ".tmp");
}

yx::Result<void> _rename_model_file(const std::filesystem::path& source,
                                    const std::filesystem::path& target)
{
  std::error_code error;
  std::filesystem::rename(source, target, error);
  if (error) {
    return yx::fail("failed to store quantized model: " + error.message());
  }

  return yx::ok();
}

yx::Result<void> _delete_stale_model_file(const std::filesystem::path& path)
{
  const auto deleted = platform::safe_delete_file(path);
  if (!deleted) {
    return yx::fail(deleted.error());
  }

  return yx::ok();
}

yx::Result<void> _run_whisper_model_quantizer(const std::filesystem::path& quantizer,
                                              const std::vector<std::string>& args)
{
  if (!store::file_exists_nonempty(quantizer)) {
    return yx::fail("missing whisper.cpp quantizer: " + quantizer.string());
  }

  const auto success = platform::run_process_blocking(args);
  if (!success) {
    return yx::fail(success.error());
  }

  if (!*success) {
    return yx::fail("failed to quantize model");
  }

  return yx::ok();
}

} // namespace

yx::Result<void> quantize_model(const std::string& name, const std::string& quant)
{
  if (!artifact::is_supported_full_model(name)) {
    return yx::fail("unsupported model size: " + name);
  }

  if (!artifact::is_supported_quant_type(quant)) {
    return yx::fail("unsupported quant type: " + quant +
                    "\nsupported quant types: " + _supported_quant_types_message());
  }

  const auto source_path = get_model_path(name);
  if (!source_path) {
    return yx::fail(source_path.error());
  }

  if (!store::file_exists_nonempty(*source_path)) {
    return yx::fail("model '" + name +
                    "' is not installed. Install it with: asryx --model install " + name);
  }

  const auto target_path = _quantized_model_path({.family = name, .quant = quant});
  if (!target_path) {
    return yx::fail(target_path.error());
  }

  const std::string target_name = artifact::quantized_model_name({.family = name, .quant = quant});
  if (store::file_exists_nonempty(*target_path)) {
    std::cout << "Model " << target_name << " is already installed.\n";
    return yx::ok();
  }

  std::filesystem::create_directories(target_path->parent_path());

  const auto tmp_path = _temporary_model_path(*target_path);
  const auto deleted = _delete_stale_model_file(tmp_path);
  if (!deleted) {
    return yx::fail(deleted.error());
  }

  const auto quantizer = store::whisper_model_quantizer();
  if (!quantizer) {
    return yx::fail(quantizer.error());
  }

  std::cout << "Quantizing model " << name << " as " << quant << "...\n";
  const auto quantized = _run_whisper_model_quantizer(
      *quantizer, {quantizer->string(), *source_path, tmp_path.string(), quant});
  if (!quantized) {
    yx::ignore_failure(_delete_stale_model_file(tmp_path));
    return yx::fail("failed to quantize " + name + " as " + quant + ": " +
                    quantized.error().message);
  }

  if (!store::file_exists_nonempty(tmp_path)) {
    yx::ignore_failure(_delete_stale_model_file(tmp_path));
    return yx::fail("quantizer did not create model: " + tmp_path.string());
  }

  const auto renamed = _rename_model_file(tmp_path, *target_path);
  if (!renamed) {
    yx::ignore_failure(_delete_stale_model_file(tmp_path));
    return yx::fail(renamed.error());
  }

  std::cout << "Quantized model installed: " << target_name << "\n";
  return yx::ok();
}

} // namespace model
