/**
 * @file towners.h
 *
 * Interface of functionality for loading and spawning towners.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include "engine/clx_sprite.hpp"
#include "items.h"
#include "levels/dun_tile.hpp"
#include "player.h"
#include "quests.h"

namespace devilution {

#define NUM_TOWNERS 16

enum _talker_id : uint8_t {
	TOWN_SMITH,
	TOWN_HEALER,
	TOWN_DEADGUY,
	TOWN_TAVERN,
	TOWN_STORY,
	TOWN_DRUNK,
	TOWN_WITCH,
	TOWN_BMAID,
	TOWN_PEGBOY,
	TOWN_COW,
	TOWN_FARMER,
	TOWN_GIRL,
	TOWN_COWFARM,
	NUM_TOWNER_TYPES,
};

extern const char *const TownerLongNames[NUM_TOWNER_TYPES];

struct Towner {
	OptionalOwnedClxSpriteList ownedAnim;
	OptionalClxSpriteList anim;
	/** Specifies the animation frame sequence. */
	std::span<const uint8_t> animOrder;
	void (*talk)(Player &player, Towner &towner);

	std::string_view name;

	/** Tile position of NPC */
	Point position;
	/** Randomly chosen topic for discussion (picked when loading into town) */
	_speech_id gossip;
	uint16_t _tAnimWidth;
	/** Tick length of each frame in the current animation */
	int16_t _tAnimDelay;
	/** Increases by one each game tick, counting how close we are to _pAnimDelay */
	int16_t _tAnimCnt;
	/** Number of frames in current animation */
	uint8_t _tAnimLen;
	/** Current frame of animation. */
	uint8_t _tAnimFrame;
	uint8_t _tAnimFrameCnt;
	_talker_id _ttype;

	[[nodiscard]] ClxSprite currentSprite() const
	{
		return (*anim)[_tAnimFrame];
	}
	[[nodiscard]] Displacement getRenderingOffset() const
	{
		return { -CalculateSpriteTileCenterX(_tAnimWidth), 0 };
	}
};

extern Towner Towners[NUM_TOWNERS];
bool IsTownerPresent(_talker_id npc);
/**
 * @brief Maps from a _talker_id value to a pointer to the Towner object, if they have been initialised
 * @param type enum constant identifying the towner
 * @return Pointer to the Towner or nullptr if they are not available
 */
Towner *GetTowner(_talker_id type);

void InitTowners();
void FreeTownerGFX();
void ProcessTowners();
void TalkToTowner(Player &player, int t);

void UpdateGirlAnimAfterQuestComplete();
void UpdateCowFarmerAnimAfterQuestComplete();

#ifdef _DEBUG
bool DebugTalkToTowner(_talker_id type);
#endif
extern _speech_id QuestDialogTable[NUM_TOWNER_TYPES][MAXQUESTS];

} // namespace devilution
