#pragma once

#include <string>

namespace devilution {

/** Optional asset substitutions supplied by a mod. */
struct PresentationRules {
	std::string startupLogo = "gendata\\logo.smk";
	std::string victoryMovie;
	std::string loadingPalette = "gendata\\cutstart.pal";
	std::string townMusic;
	std::string skullLeverName;
	std::string bookcaseName;
};

inline PresentationRules Presentation;

void LoadPresentationRules();

} // namespace devilution
