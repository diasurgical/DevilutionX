#include <cstddef>

#include "DiabloUI/support_lines.h"
#include "utils/language.h"

namespace devilution {

const char *const SupportLines[] = {
	"",
	N_("We maintain a chat server at Discord.gg/devilutionx Follow the links to join our community where we talk about things related to Diablo, and the Hellfire expansion."),
	"",
	N_("DevilutionX is maintained by Diasurgical, issues and bugs can be reported at this address: https://github.com/diasurgical/devilutionX To help us better serve you, please be sure to include the version number, operating system, and the nature of the problem."),
	"",
	"",
	N_("Disclaimer:"),
	N_("	DevilutionX is not supported or maintained by Blizzard Entertainment, nor GOG.com. Neither Blizzard Entertainment nor GOG.com has tested or certified the quality or compatibility of DevilutionX. All inquiries regarding DevilutionX should be directed to Diasurgical, not to Blizzard Entertainment or GOG.com."),
	"",
	"",
	N_("	This port makes use of Charis SIL, New Athena Unicode, Unifont, and Noto which are licensed under the SIL Open Font License, as well as Twitmoji which is licensed under CC-BY 4.0. The port also makes use of SDL which is licensed under the zlib-license. See the ReadMe for further details."),
	"",
	"",
};

const std::size_t SupportLinesSize = sizeof(SupportLines) / sizeof(SupportLines[0]);

} // namespace devilution
