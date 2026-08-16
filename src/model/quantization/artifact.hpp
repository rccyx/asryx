#ifndef ASRYX_MODEL_QUANTIZATION_ARTIFACT_HPP
#define ASRYX_MODEL_QUANTIZATION_ARTIFACT_HPP

#include "error.hpp"

#include <string>
#include <vector>

namespace model::artifact {

struct ModelArtifact
{
  std::string family;
  std::string quant;
};

const std::vector<std::string>& supported_quant_types();
bool is_supported_full_model(const std::string& name);
bool is_supported_quant_type(const std::string& quant);
bool is_supported_model_artifact(const std::string& name);
bool is_english_only_artifact(const std::string& name);
std::string quantized_model_name(const ModelArtifact& artifact);
yx::Result<ModelArtifact> parse_quantized_model(const std::string& name);

} // namespace model::artifact

#endif // ASRYX_MODEL_QUANTIZATION_ARTIFACT_HPP
