#ifndef ASRYX_MODEL_MODEL_HPP
#define ASRYX_MODEL_MODEL_HPP

#include "error.hpp"

#include <expected>
#include <string>
#include <vector>

namespace config {
struct Config;
} // namespace config

namespace model {

const std::vector<std::string>& get_supported_models();
const std::vector<std::string>& get_supported_languages();
std::expected<std::string, asryx::Error> get_model_path(const std::string& name);
std::expected<std::string, asryx::Error> get_vad_model_path();
std::expected<bool, asryx::Error> is_model_installed(const std::string& name);
std::expected<bool, asryx::Error> is_vad_model_installed();
bool is_supported_language(const std::string& language);
bool is_english_only_model(const std::string& name);
std::expected<void, asryx::Error> validate_config(const config::Config& cfg);
std::expected<void, asryx::Error> validate_vad_model();
std::expected<std::string, asryx::Error> transcription_language_for(const config::Config& cfg);
std::expected<void, asryx::Error> list_models();
std::expected<void, asryx::Error> install_model(const std::string& name);
std::expected<void, asryx::Error> use_model(const std::string& name);
std::expected<void, asryx::Error> use_language(const std::string& language);
std::expected<void, asryx::Error> uninstall_model(const std::string& name);

} // namespace model

#endif // ASRYX_MODEL_MODEL_HPP
