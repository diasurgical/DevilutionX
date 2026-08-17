#include <string>
#include <string_view>
#include <vector>

std::string_view GetLanguageCode() { return "en"; }
std::string_view GetVoiceLanguageCode() { return "en"; }
bool HasTranslation(const std::string &locale) { return true; }
void LanguageInitialize() { }
std::string_view LanguageTranslate(const char *key) { return key; }
std::string_view LanguagePluralTranslate(const char *singular, std::string_view plural, int count) { return count == 1 ? singular : plural; }
std::string_view LanguageParticularTranslate(std::string_view context, std::string_view message) { return message; }

namespace devilution {
std::vector<std::string> GetLocales() { return {}; }
} // namespace devilution
