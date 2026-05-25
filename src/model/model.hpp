#ifndef ASRYX_MODEL_MODEL_HPP
#define ASRYX_MODEL_MODEL_HPP

#include <string>
#include <vector>

namespace config {
struct Config;
} // namespace config

namespace model {

const std::vector<std::string>& get_supported_models();
const std::vector<std::string>& get_supported_languages();
std::string get_model_path(const std::string& name);
bool is_model_installed(const std::string& name);
bool is_supported_language(const std::string& language);
bool is_english_only_model(const std::string& name);
void validate_config(const config::Config& cfg);
std::string transcription_language_for(const config::Config& cfg);
void list_models();
void install_model(const std::string& name);
void use_model(const std::string& name);
void use_language(const std::string& language);
void uninstall_model(const std::string& name);

} // namespace model

#endif // ASRYX_MODEL_MODEL_HPP
