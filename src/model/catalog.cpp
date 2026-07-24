#include "model/model.hpp"

#include <string>
#include <vector>

namespace model {

const std::vector<std::string>& get_supported_models()
{
  static const std::vector<std::string> models = {
      "tiny.en", "tiny",     "base.en",  "base",     "small.en",       "small", "medium.en",
      "medium",  "large-v1", "large-v2", "large-v3", "large-v3-turbo", "large"};
  return models;
}

const std::vector<std::string>& get_supported_languages()
{
  static const std::vector<std::string> languages = {
      "en", "zh", "de", "es",  "ru", "ko", "fr", "ja", "pt", "tr", "pl", "ca", "nl", "ar", "sv",
      "it", "id", "hi", "fi",  "vi", "he", "uk", "el", "ms", "cs", "ro", "da", "hu", "ta", "no",
      "th", "ur", "hr", "bg",  "lt", "la", "mi", "ml", "cy", "sk", "te", "fa", "lv", "bn", "sr",
      "az", "sl", "kn", "et",  "mk", "br", "eu", "is", "hy", "ne", "mn", "bs", "kk", "sq", "sw",
      "gl", "mr", "pa", "si",  "km", "sn", "yo", "so", "af", "oc", "ka", "be", "tg", "sd", "gu",
      "am", "yi", "lo", "uz",  "fo", "ht", "ps", "tk", "nn", "mt", "sa", "lb", "my", "bo", "tl",
      "mg", "as", "tt", "haw", "ln", "ha", "ba", "jw", "su", "yue"};
  return languages;
}

} // namespace model
