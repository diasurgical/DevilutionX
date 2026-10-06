/**
 * @file debug.cpp
 *
 * Implementation of debug functions.
 */

#ifdef _DEBUG

#include <cmath>
#include <cstdint>
#include <cstdio>

#include <ankerl/unordered_dense.h>

#include "debug.h"

#include "automap.h"
#include "cursor.h"
#include "engine/load_cel.hpp"
#include "engine/point.hpp"
#include "lighting.h"
#include "missiles.h"
#include "monster.h"
#include "plrmsg.h"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

std::string TestMapPath;
OptionalOwnedClxSpriteList pSquareCel;
bool DebugToggle = false;
bool DebugGodMode = false;
bool DebugInvisible = false;
bool DebugVision = false;
bool DebugPath = false;
bool DebugGrid = false;
ankerl::unordered_dense::map<int, Point> DebugCoordsMap;
bool DebugScrollViewEnabled = false;
std::string debugTRN;

// Used for debugging level generation
uint32_t glMid1Seed[NUMLEVELS];
uint32_t glMid2Seed[NUMLEVELS];
uint32_t glMid3Seed[NUMLEVELS];
uint32_t glEndSeed[NUMLEVELS];

namespace {

DebugGridTextItem SelectedDebugGridTextItem;

int DebugMonsterId;

std::vector<std::string> SearchMonsters;
std::vector<std::string> SearchItems;
std::vector<std::string> SearchObjects;

void PrintDebugMonster(const Monster &monster)
{
	EventPlrMsg(StrCat(
	                "Monster ", static_cast<int>(monster.getId()), " = ", monster.name(),
	                "\nX = ", monster.position.tile.x, ", Y = ", monster.position.tile.y,
	                "\nEnemy = ", monster.enemy, ", HP = ", monster.hitPoints,
	                "\nMode = ", static_cast<int>(monster.mode), ", Var1 = ", monster.var1),
	    UiFlags::ColorWhite);

	bool bActive = false;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		if (&Monsters[ActiveMonsters[i]] == &monster) {
			bActive = true;
			break;
		}
	}

	EventPlrMsg(StrCat("Active List = ", bActive ? 1 : 0, ", Squelch = ", monster.activeForTicks), UiFlags::ColorWhite);
}

} // namespace

void LoadDebugGFX()
{
	pSquareCel = LoadCel("data\\square", 64);
}

void FreeDebugGFX()
{
	pSquareCel = std::nullopt;
}

void GetDebugMonster()
{
	int monsterIndex = pcursmonst;
	if (monsterIndex == -1)
		monsterIndex = std::abs(dMonster[cursPosition.x][cursPosition.y]) - 1;

	if (monsterIndex == -1)
		monsterIndex = DebugMonsterId;

	PrintDebugMonster(Monsters[monsterIndex]);
}

void NextDebugMonster()
{
	DebugMonsterId++;
	if (DebugMonsterId == MaxMonsters)
		DebugMonsterId = 0;

	EventPlrMsg(StrCat("Current debug monster = ", DebugMonsterId), UiFlags::ColorWhite);
}

void SetDebugLevelSeedInfos(uint32_t mid1Seed, uint32_t mid2Seed, uint32_t mid3Seed, uint32_t endSeed)
{
	glMid1Seed[currlevel] = mid1Seed;
	glMid2Seed[currlevel] = mid2Seed;
	glMid3Seed[currlevel] = mid3Seed;
	glEndSeed[currlevel] = endSeed;
}

bool IsDebugGridTextNeeded()
{
	return SelectedDebugGridTextItem != DebugGridTextItem::None;
}

bool IsDebugGridInMegatiles()
{
	switch (SelectedDebugGridTextItem) {
	case DebugGridTextItem::AutomapView:
	case DebugGridTextItem::dungeon:
	case DebugGridTextItem::pdungeon:
	case DebugGridTextItem::Protected:
		return true;
	default:
		return false;
	}
}

DebugGridTextItem GetDebugGridTextType()
{
	return SelectedDebugGridTextItem;
}

void SetDebugGridTextType(DebugGridTextItem value)
{
	SelectedDebugGridTextItem = value;
}

bool GetDebugGridText(Point dungeonCoords, std::string &debugGridText)
{
	int info = 0;
	int blankValue = 0;
	debugGridText.clear();
	Point megaCoords = dungeonCoords.worldToMega();
	switch (SelectedDebugGridTextItem) {
	case DebugGridTextItem::coords:
		StrAppend(debugGridText, dungeonCoords.x, ":", dungeonCoords.y);
		return true;
	case DebugGridTextItem::cursorcoords:
		if (dungeonCoords != cursPosition)
			return false;
		StrAppend(debugGridText, dungeonCoords.x, ":", dungeonCoords.y);
		return true;
	case DebugGridTextItem::objectindex: {
		info = 0;
		Object *object = FindObjectAtPosition(dungeonCoords);
		if (object != nullptr) {
			info = static_cast<int>(object->_otype);
		}
		break;
	}
	case DebugGridTextItem::microTiles: {
		const MICROS &micros = DPieceMicros[dPiece[dungeonCoords.x][dungeonCoords.y]];
		for (const LevelCelBlock tile : micros.mt) {
			if (!tile.hasValue()) break;
			if (!debugGridText.empty()) debugGridText += '\n';
			StrAppend(debugGridText, tile.frame(), " ");
			switch (tile.type()) {
			case TileType::Square: StrAppend(debugGridText, "S"); break;
			case TileType::TransparentSquare: StrAppend(debugGridText, "T"); break;
			case TileType::LeftTriangle: StrAppend(debugGridText, "<"); break;
			case TileType::RightTriangle: StrAppend(debugGridText, ">"); break;
			case TileType::LeftTrapezoid: StrAppend(debugGridText, "\\"); break;
			case TileType::RightTrapezoid: StrAppend(debugGridText, "/"); break;
			}
		}
		return !debugGridText.empty();
	} break;
	case DebugGridTextItem::dPiece:
		info = dPiece[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dTransVal:
		info = dTransVal[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dLight:
		info = dLight[dungeonCoords.x][dungeonCoords.y];
		blankValue = LightsMax;
		break;
	case DebugGridTextItem::dPreLight:
		info = dPreLight[dungeonCoords.x][dungeonCoords.y];
		blankValue = LightsMax;
		break;
	case DebugGridTextItem::dFlags:
		info = static_cast<int>(dFlags[dungeonCoords.x][dungeonCoords.y]);
		break;
	case DebugGridTextItem::dPlayer:
		info = dPlayer[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dMonster:
		info = dMonster[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::missiles: {
		for (auto &missile : Missiles) {
			if (missile.position.tile == dungeonCoords) {
				if (!debugGridText.empty()) debugGridText += '\n';
				debugGridText.append(std::to_string((int)missile._mitype));
			}
		}
		return !debugGridText.empty();
	} break;
	case DebugGridTextItem::dCorpse:
		info = dCorpse[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dItem:
		info = dItem[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dSpecial:
		info = dSpecial[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dObject:
		info = dObject[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::Solid:
		info = TileHasAny(dungeonCoords, TileProperties::Solid) << 0 | TileHasAny(dungeonCoords, TileProperties::BlockLight) << 1 | TileHasAny(dungeonCoords, TileProperties::BlockMissile) << 2;
		break;
	case DebugGridTextItem::Transparent:
		info = TileHasAny(dungeonCoords, TileProperties::Transparent) << 0 | TileHasAny(dungeonCoords, TileProperties::TransparentLeft) << 1 | TileHasAny(dungeonCoords, TileProperties::TransparentRight) << 2;
		break;
	case DebugGridTextItem::Trap:
		info = TileHasAny(dungeonCoords, TileProperties::Trap);
		break;
	case DebugGridTextItem::AutomapView:
		if (megaCoords.x >= 0 && megaCoords.x < DMAXX && megaCoords.y >= 0 && megaCoords.y < DMAXY)
			info = AutomapView[megaCoords.x][megaCoords.y];
		break;
	case DebugGridTextItem::dungeon:
		if (megaCoords.x >= 0 && megaCoords.x < DMAXX && megaCoords.y >= 0 && megaCoords.y < DMAXY)
			info = dungeon[megaCoords.x][megaCoords.y];
		break;
	case DebugGridTextItem::pdungeon:
		if (megaCoords.x >= 0 && megaCoords.x < DMAXX && megaCoords.y >= 0 && megaCoords.y < DMAXY)
			info = pdungeon[megaCoords.x][megaCoords.y];
		break;
	case DebugGridTextItem::Protected:
		if (megaCoords.x >= 0 && megaCoords.x < DMAXX && megaCoords.y >= 0 && megaCoords.y < DMAXY)
			info = Protected.test(megaCoords.x, megaCoords.y);
		break;
	case DebugGridTextItem::None:
		return false;
	}
	if (info == blankValue)
		return false;
	StrAppend(debugGridText, info);
	return true;
}

bool IsDebugAutomapHighlightNeeded()
{
	return SearchMonsters.size() > 0 || SearchItems.size() > 0 || SearchObjects.size() > 0;
}

bool ShouldHighlightDebugAutomapTile(Point position)
{
	auto matchesSearched = [](const std::string_view name, const std::vector<std::string> &searchedNames) {
		const std::string lowercaseName = AsciiStrToLower(name);
		for (const auto &searchedName : searchedNames) {
			if (lowercaseName.find(searchedName) != std::string::npos) {
				return true;
			}
		}
		return false;
	};

	if (SearchMonsters.size() > 0 && dMonster[position.x][position.y] != 0) {
		const int mi = std::abs(dMonster[position.x][position.y]) - 1;
		const Monster &monster = Monsters[mi];
		if (matchesSearched(monster.name(), SearchMonsters))
			return true;
	}

	if (SearchItems.size() > 0 && dItem[position.x][position.y] != 0) {
		const int itemId = std::abs(dItem[position.x][position.y]) - 1;
		const Item &item = Items[itemId];
		if (matchesSearched(item.getName(), SearchItems))
			return true;
	}

	if (SearchObjects.size() > 0 && IsObjectAtPosition(position)) {
		const Object &object = ObjectAtPosition(position);
		if (matchesSearched(object.name(), SearchObjects))
			return true;
	}

	return false;
}

void AddDebugAutomapMonsterHighlight(std::string_view name)
{
	SearchMonsters.emplace_back(name);
}

void AddDebugAutomapItemHighlight(std::string_view name)
{
	SearchItems.emplace_back(name);
}

void AddDebugAutomapObjectHighlight(std::string_view name)
{
	SearchObjects.emplace_back(name);
}

void ClearDebugAutomapHighlights()
{
	SearchMonsters.clear();
	SearchItems.clear();
	SearchObjects.clear();
}

} // namespace devilution

#endif
