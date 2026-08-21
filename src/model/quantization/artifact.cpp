#include "model/quantization/artifact.hpp"

#include "model/model.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace model::artifact {

const std::vector<std::string>& supported_quant_types()
{
  static const std::vector<std::string> types = {"q4_0", "q4_1", "q5_0", "q5_1", "q8_0",
                                                 "q2_k", "q3_k", "q4_k", "q5_k", "q6_k"};
  return types;
}

bool is_supported_full_model(const std::string& name)
{
  const auto& models = get_supported_models();
  return std::find(models.begin(), models.end(), name) != models.end();
}

bool is_supported_quant_type(const std::string& quant)
{
  const auto& types = supported_quant_types();
  return std::find(types.begin(), types.end(), quant) != types.end();
}

bool is_supported_model_artifact(const std::string& name)
{
  return is_supported_full_model(name) || parse_quantized_model(name).has_value();
}

bool is_english_only_artifact(const std::string& name)
{
  const auto artifact = parse_quantized_model(name);
  const std::string& family = artifact ? artifact->family : name;
  return family == "tiny.en" || family == "base.en" || family == "small.en" ||
         family == "medium.en";
}

std::string quantized_model_name(const ModelArtifact& artifact)
{
  return artifact.family + "-" + artifact.quant;
}

yx::Result<ModelArtifact> parse_quantized_model(const std::string& name)
{
  for (const auto& quant : supported_quant_types()) {
    const std::string suffix = "-" + quant;
    if (!name.ends_with(suffix)) {
      continue;
    }

    const std::string family = name.substr(0, name.size() - suffix.size());
    if (!is_supported_full_model(family)) {
      return yx::fail("unsupported model size: " + name);
    }

    return yx::ok(ModelArtifact{.family = family, .quant = quant});
  }

  return yx::fail("unsupported model size: " + name);
}

} // namespace model::artifact
