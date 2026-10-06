#include "levels/drlg_quests.hpp"

#include <cstdint>

#include "engine/load_file.hpp"
#include "engine/world_tile.hpp"
#include "levels/gendung.h"
#include "tables/questdat.hpp"
#include "utils/endian_swap.hpp"

namespace devilution {

namespace {

/**
 * @brief There is no reason to run this, the room has already had a proper sector assigned
 */
void DrawButcher()
{
	const Point position = SetPiece.position.megaToWorld() + Displacement { 3, 3 };
	DRLG_RectTrans({ position, { 7, 7 } });
}

void DrawSkelKing(quest_id q, Point position)
{
	Quests[q].position = position.megaToWorld() + Displacement { 12, 7 };
}

void DrawWarLord(Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("levels\\l4data\\warlord2.dun");

	SetPiece = { position, GetDunSize(dunData.get()) };

	PlaceDunTiles(dunData.get(), position, 6);
}

void DrawSChamber(quest_id q, Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("levels\\l2data\\bonestr1.dun");

	SetPiece = { position, GetDunSize(dunData.get()) };

	PlaceDunTiles(dunData.get(), position, 3);

	Quests[q].position = position.megaToWorld() + Displacement { 6, 7 };
}

void DrawLTBanner(Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("levels\\l1data\\banner1.dun");

	const WorldTileSize size = GetDunSize(dunData.get());

	SetPiece = { position, size };

	const uint16_t *tileLayer = &dunData[2];

	for (WorldTileCoord j = 0; j < size.height; j++) {
		for (WorldTileCoord i = 0; i < size.width; i++) {
			auto tileId = static_cast<uint8_t>(Swap16LE(tileLayer[(j * size.width) + i]));
			if (tileId != 0) {
				pdungeon[position.x + i][position.y + j] = tileId;
			}
		}
	}
}

/**
 * Close outer wall
 */
void DrawBlind(Point position)
{
	dungeon[position.x][position.y + 1] = 154;
	dungeon[position.x + 10][position.y + 8] = 154;
}

void DrawBlood(Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("levels\\l2data\\blood2.dun");

	SetPiece = { position, GetDunSize(dunData.get()) };

	PlaceDunTiles(dunData.get(), position, 0);
}

} // namespace

void DRLG_CheckQuests(Point position)
{
	for (auto &quest : Quests) {
		if (quest.IsAvailable()) {
			switch (quest._qidx) {
			case Q_BUTCHER:
				DrawButcher();
				break;
			case Q_LTBANNER:
				DrawLTBanner(position);
				break;
			case Q_BLIND:
				DrawBlind(position);
				break;
			case Q_BLOOD:
				DrawBlood(position);
				break;
			case Q_WARLORD:
				DrawWarLord(position);
				break;
			case Q_SKELKING:
				DrawSkelKing(quest._qidx, position);
				break;
			case Q_SCHAMB:
				DrawSChamber(quest._qidx, position);
				break;
			default:
				break;
			}
		}
	}
}

} // namespace devilution
