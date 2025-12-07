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
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "engine/clx_sprite.hpp"
#include "items.h"
#include "levels/dun_tile.hpp"
#include "player.h"
#include "quests.h"
#include "utils/attributes.h"

namespace devilution {

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
	// Note: Enum values are parsed from TSV using magic_enum
	// The actual count is determined dynamically from TSV data
};

// Runtime mappings built from TSV data
extern DVL_API_FOR_TEST std::unordered_map<_talker_id, std::string> TownerLongNames; // Maps towner type enum to display name

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

extern std::vector<Towner> Towners;

/**
 * @brief Returns the number of unique towner types found in TSV data.
 * This is dynamically determined from the loaded towner data.
 */
size_t GetNumTownerTypes();

/**
 * @brief Returns the number of towner instances (actual spawned towners).
 * This is dynamically determined from the loaded towner data.
 */
size_t GetNumTowners();

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

} // namespace devilution
