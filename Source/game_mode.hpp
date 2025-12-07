#pragma once

#include "utils/attributes.h"

namespace devilution {

/* Are we in-game? If false, we're in the main menu. */
extern DVL_API_FOR_TEST bool gbRunGame;
/** Indicate if we only have access to demo data */
extern DVL_API_FOR_TEST bool gbIsSpawn;
/** Indicate if we have loaded the Hellfire expansion data */
extern DVL_API_FOR_TEST bool gbIsHellfire;
/** Indicate if we want vanilla savefiles */
extern DVL_API_FOR_TEST bool gbVanilla;
/** Whether the Hellfire mode is required (forced). */
extern bool forceHellfire;

} // namespace devilution
