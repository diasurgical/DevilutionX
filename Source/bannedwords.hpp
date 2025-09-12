// bannedwords.hpp

#include <string>
#include <vector>

namespace devilution {

void LoadBannedWords();
std::string CensorMessage(const std::string &original);

extern std::vector<std::string> BannedWords;

} // namespace devilution
