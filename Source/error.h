/**
 * @file error.h
 *
 * Interface of in-game message functions.
 */
#pragma once

#include <cstdint>
#include <string>

#include "engine.h"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {

enum diablo_message : uint8_t {
	EMSG_NONE,
	EMSG_SAVED_GAME,
	EMSG_NO_MULTIPLAYER_IN_DEMO,
	EMSG_DIRECT_SOUND_FAILED,
	EMSG_NOT_IN_SHAREWARE,
	EMSG_NO_SPACE_TO_SAVE,
	EMSG_NO_PAUSE_IN_TOWN,
	EMSG_COPY_TO_HDD,
	EMSG_DESYNC,
	EMSG_NO_PAUSE_IN_MP,
	EMSG_LOADING,
	EMSG_SAVING,
	EMSG_SHRINE_MYSTERIOUS,
	EMSG_SHRINE_HIDDEN,
	EMSG_SHRINE_GLOOMY,
	EMSG_SHRINE_WEIRD,
	EMSG_SHRINE_MAGICAL,
	EMSG_SHRINE_STONE,
	EMSG_SHRINE_RELIGIOUS,
	EMSG_SHRINE_ENCHANTED,
	EMSG_SHRINE_THAUMATURGIC,
	EMSG_SHRINE_FASCINATING,
	EMSG_SHRINE_CRYPTIC,
	EMSG_SHRINE_UNUSED,
	EMSG_SHRINE_ELDRITCH,
	EMSG_SHRINE_EERIE,
	EMSG_SHRINE_DIVINE,
	EMSG_SHRINE_HOLY,
	EMSG_SHRINE_SACRED,
	EMSG_SHRINE_SPIRITUAL,
	EMSG_SHRINE_SPOOKY1,
	EMSG_SHRINE_SPOOKY2,
	EMSG_SHRINE_ABANDONED,
	EMSG_SHRINE_CREEPY,
	EMSG_SHRINE_QUIET,
	EMSG_SHRINE_SECLUDED,
	EMSG_SHRINE_ORNATE,
	EMSG_SHRINE_GLIMMERING,
	EMSG_SHRINE_TAINTED1,
	EMSG_SHRINE_TAINTED2,
	EMSG_REQUIRES_LVL_8,
	EMSG_REQUIRES_LVL_13,
	EMSG_REQUIRES_LVL_17,
	EMSG_BONECHAMB,
	EMSG_SHRINE_OILY,
	EMSG_SHRINE_GLOWING,
	EMSG_SHRINE_MENDICANT,
	EMSG_SHRINE_SPARKLING,
	EMSG_SHRINE_TOWN,
	EMSG_SHRINE_SHIMMERING,
	EMSG_SHRINE_SOLAR1,
	EMSG_SHRINE_SOLAR2,
	EMSG_SHRINE_SOLAR3,
	EMSG_SHRINE_SOLAR4,
	EMSG_SHRINE_MURPHYS,
};

void InitDiabloMsg(diablo_message e);
void InitDiabloMsg(string_view msg);
bool IsDiabloMsgAvailable();
void CancelCurrentDiabloMsg();
void ClrDiabloMsg();
void DrawDiabloMsg(const Surface &out);

} // namespace devilution
