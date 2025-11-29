/**
 * @file townerdat.hpp
 *
 * Interface for loading towner data from TSV files.
 */
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "engine/direction.hpp"
#include "levels/gendung.h"
#include "objdat.h"
#include "textdat.h"
#include "towners.h"
#include "utils/attributes.h"

namespace devilution {

/**
 * @brief Data for a single towner entry loaded from TSV.
 */
struct TownerDataEntry {
	_talker_id type; // Parsed from TSV using magic_enum
	std::string name;
	Point position;
	Direction direction;
	uint16_t animWidth;
	std::string animPath;
	uint8_t animFrames;
	int16_t animDelay;
	std::vector<_speech_id> gossipTexts;
	std::vector<uint8_t> animOrder;
};

/** Contains the data for all towners loaded from TSV. */
extern DVL_API_FOR_TEST std::vector<TownerDataEntry> TownersDataEntries;

/** Contains the quest dialog table loaded from TSV. Indexed by [towner_type][quest_id]. */
extern std::unordered_map<_talker_id, std::array<_speech_id, MAXQUESTS>> TownerQuestDialogTable;

/**
 * @brief Loads towner data from TSV files.
 *
 * This function loads data from:
 * - txtdata/towners/towners.tsv - Main towner definitions
 * - txtdata/towners/quest_dialog.tsv - Quest dialog mappings
 */
void LoadTownerData();

/**
 * @brief Gets the quest dialog speech ID for a towner and quest combination.
 * @param type The towner type
 * @param quest The quest ID
 * @return The speech ID for the dialog, or TEXT_NONE if not available
 */
_speech_id GetTownerQuestDialog(_talker_id type, quest_id quest);

/**
 * @brief Sets the quest dialog speech ID for a towner and quest combination.
 * @param type The towner type
 * @param quest The quest ID
 * @param speech The speech ID to set
 */
void SetTownerQuestDialog(_talker_id type, quest_id quest, _speech_id speech);

} // namespace devilution
