#ifndef ASRYX_MODEL_MODEL_HPP
#define ASRYX_MODEL_MODEL_HPP

#include "error.hpp"

#include <string>
#include <vector>

namespace config {
struct Config;
} // namespace config

namespace model {

const std::vector<std::string>& get_supported_models();
const std::vector<std::string>& get_supported_languages();
yx::Result<std::string> get_model_path(const std::string& name);
yx::Result<std::string> get_vad_model_path();
yx::Result<bool> is_model_installed(const std::string& name);
yx::Result<bool> is_vad_model_installed();
bool is_supported_language(const std::string& language);
bool is_english_only_model(const std::string& name);
yx::Result<void> validate_config(const config::Config& cfg);
yx::Result<void> validate_vad_model();
yx::Result<std::string> transcription_language_for(const config::Config& cfg);
yx::Result<void> list_models();
yx::Result<void> install_model(const std::string& name);
yx::Result<void> use_model(const std::string& name);
yx::Result<void> use_language(const std::string& language);
yx::Result<void> uninstall_model(const std::string& name);

} // namespace model

#endif // ASRYX_MODEL_MODEL_HPP
