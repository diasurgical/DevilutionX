#include "engine/trn.hpp"

#include <cstdint>

#include <fmt/format.h>

#ifdef _DEBUG
#include "debug.h"
#endif
#include "engine/load_file.hpp"
#include "lighting.h"
#include "utils/str_cat.hpp"

namespace devilution {

uint8_t *GetInfravisionTRN()
{
	return InfravisionTable.data();
}

uint8_t *GetStoneTRN()
{
	return StoneTable.data();
}

uint8_t *GetPauseTRN()
{
	return PauseTable.data();
}

std::optional<std::array<uint8_t, 256>> GetClassTRN(Player &player)
{
	std::array<uint8_t, 256> trn;
	char path[64];

	const PlayerSpriteData &spriteData = GetPlayerSpriteDataForClass(player._pClass);
	*BufCopy(path, "plrgfx\\", spriteData.trn, ".trn") = '\0';

#ifdef _DEBUG
	if (!debugTRN.empty()) {
		*BufCopy(path, debugTRN.c_str()) = '\0';
	}
#endif
	if (LoadOptionalFileInMem(path, &trn[0], 256)) {
		return trn;
	}
	return std::nullopt;
}

std::optional<std::array<uint8_t, 256>> GetPlayerGraphicTRN(const char *pszName)
{
	char path[MaxMpqPathSize];
	*BufCopy(path, pszName, ".trn") = '\0';

	std::array<uint8_t, 256> trn;
	if (LoadOptionalFileInMem(path, &trn[0], 256)) {
		return trn;
	}
	return std::nullopt;
}

} // namespace devilution
