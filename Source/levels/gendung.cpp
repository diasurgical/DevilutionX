#include "levels/gendung.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stack>
#include <string>
#include <utility>
#include <vector>

#include <expected.hpp>

#include "engine/clx_sprite.hpp"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "engine/world_tile.hpp"
#include "game_mode.hpp"
#include "items.h"
#include "levels/drlg_l1.h"
#include "levels/drlg_l2.h"
#include "levels/drlg_l3.h"
#include "levels/drlg_l4.h"
#include "levels/town.h"
#include "lighting.h"
#include "monster.h"
#include "objects.h"
#include "utils/bitset2d.hpp"
#include "utils/endian_swap.hpp"
#include "utils/is_of.hpp"
#include "utils/log.hpp"

namespace devilution {

namespace {

/**
 * @brief Starting from the origin point determine how much floor space is available with the given bounds
 *
 * Essentially looks for the widest/tallest rectangular area of at least the minimum size, but due to a weird/buggy
 * bounds check can return an area smaller than the available width/height.
 *
 * @param floor what value defines floor tiles within a dungeon
 * @param origin starting point for the search
 * @param minSize minimum allowable value for both dimensions
 * @param maxSize maximum allowable value for both dimensions
 * @return how much width/height is available for a theme room or an empty optional if there's not enough space
 */
std::optional<WorldTileSize> GetSizeForThemeRoom(uint8_t floor, WorldTilePosition origin, WorldTileCoord minSize, WorldTileCoord maxSize)
{
	if (origin.x + maxSize > DMAXX && origin.y + maxSize > DMAXY) {
		return {}; // Original broken bounds check, avoids lower right corner
	}
	if (IsNearThemeRoom(origin)) {
		return {};
	}

	const WorldTileCoord maxWidth = std::min<WorldTileCoord>(maxSize, DMAXX - origin.x);
	const WorldTileCoord maxHeight = std::min<WorldTileCoord>(maxSize, DMAXY - origin.y);

	WorldTileSize room { maxWidth, maxHeight };

	for (WorldTileCoord i = 0; i < maxSize; i++) {
		WorldTileCoord width = i < room.height ? i : 0;
		if (i < maxHeight) {
			while (width < room.width) {
				if (dungeon[origin.x + width][origin.y + i] != floor)
					break;

				width++;
			}
		}

		WorldTileCoord height = i < room.width ? i : 0;
		if (i < maxWidth) {
			while (height < room.height) {
				if (dungeon[origin.x + i][origin.y + height] != floor)
					break;

				height++;
			}
		}

		if (width < minSize || height < minSize) {
			if (i < minSize)
				return {};
			break;
		}

		room = { std::min(room.width, width), std::min(room.height, height) };
	}

	return room - 2;
}

void CreateThemeRoom(int themeIndex)
{
	const int lx = themeLoc[themeIndex].room.position.x;
	const int ly = themeLoc[themeIndex].room.position.y;
	const int hx = lx + themeLoc[themeIndex].room.size.width;
	const int hy = ly + themeLoc[themeIndex].room.size.height;

	for (int yy = ly; yy < hy; yy++) {
		for (int xx = lx; xx < hx; xx++) {
			if (leveltype == DTYPE_CATACOMBS) {
				if (yy == ly || yy == hy - 1) {
					dungeon[xx][yy] = 2;
				} else if (xx == lx || xx == hx - 1) {
					dungeon[xx][yy] = 1;
				} else {
					dungeon[xx][yy] = 3;
				}
			}
			if (IsAnyOf(leveltype, DTYPE_CAVES, DTYPE_NEST)) {
				if (yy == ly || yy == hy - 1) {
					dungeon[xx][yy] = 134;
				} else if (xx == lx || xx == hx - 1) {
					dungeon[xx][yy] = 137;
				} else {
					dungeon[xx][yy] = 7;
				}
			}
			if (leveltype == DTYPE_HELL) {
				if (yy == ly || yy == hy - 1) {
					dungeon[xx][yy] = 2;
				} else if (xx == lx || xx == hx - 1) {
					dungeon[xx][yy] = 1;
				} else {
					dungeon[xx][yy] = 6;
				}
			}
		}
	}

	if (leveltype == DTYPE_CATACOMBS) {
		dungeon[lx][ly] = 8;
		dungeon[hx - 1][ly] = 7;
		dungeon[lx][hy - 1] = 9;
		dungeon[hx - 1][hy - 1] = 6;
	}
	if (IsAnyOf(leveltype, DTYPE_CAVES, DTYPE_NEST)) {
		dungeon[lx][ly] = 150;
		dungeon[hx - 1][ly] = 151;
		dungeon[lx][hy - 1] = 152;
		dungeon[hx - 1][hy - 1] = 138;
	}
	if (leveltype == DTYPE_HELL) {
		dungeon[lx][ly] = 9;
		dungeon[hx - 1][ly] = 16;
		dungeon[lx][hy - 1] = 15;
		dungeon[hx - 1][hy - 1] = 12;
	}

	if (leveltype == DTYPE_CATACOMBS) {
		if (FlipCoin())
			dungeon[hx - 1][(ly + hy) / 2] = 4;
		else
			dungeon[(lx + hx) / 2][hy - 1] = 5;
	}
	if (IsAnyOf(leveltype, DTYPE_CAVES, DTYPE_NEST)) {
		if (FlipCoin())
			dungeon[hx - 1][(ly + hy) / 2] = 147;
		else
			dungeon[(lx + hx) / 2][hy - 1] = 146;
	}
	if (leveltype == DTYPE_HELL) {
		if (FlipCoin()) {
			const int yy = (ly + hy) / 2;
			dungeon[hx - 1][yy - 1] = 53;
			dungeon[hx - 1][yy] = 6;
			dungeon[hx - 1][yy + 1] = 52;
			dungeon[hx - 2][yy - 1] = 54;
		} else {
			const int xx = (lx + hx) / 2;
			dungeon[xx - 1][hy - 1] = 57;
			dungeon[xx][hy - 1] = 6;
			dungeon[xx + 1][hy - 1] = 56;
			dungeon[xx][hy - 2] = 59;
			dungeon[xx - 1][hy - 2] = 58;
		}
	}
}

bool IsFloor(Point p, uint8_t floorID)
{
	const int i = (p.x - 16) / 2;
	const int j = (p.y - 16) / 2;
	if (i < 0 || i >= DMAXX)
		return false;
	if (j < 0 || j >= DMAXY)
		return false;
	return dungeon[i][j] == floorID;
}

void FillTransparencyValues(Point floor, uint8_t floorID)
{
	const Direction allDirections[] = {
		Direction::North,
		Direction::South,
		Direction::East,
		Direction::West,
		Direction::NorthEast,
		Direction::NorthWest,
		Direction::SouthEast,
		Direction::SouthWest,
	};

	// We only fill in the surrounding tiles if they are not floor tiles
	// because they would otherwise not be visited by the span filling algorithm
	for (const Direction dir : allDirections) {
		const Point adjacent = floor + dir;
		if (!IsFloor(adjacent, floorID))
			dTransVal[adjacent.x][adjacent.y] = TransVal;
	}

	dTransVal[floor.x][floor.y] = TransVal;
}

void FindTransparencyValues(Point floor, uint8_t floorID)
{
	// Algorithm adapted from https://en.wikipedia.org/wiki/Flood_fill#Span_Filling
	// Modified to include diagonally adjacent tiles that would otherwise not be visited
	// Also, Wikipedia's selection for the initial seed is incorrect
	struct Seed {
		int scanStart;
		int scanEnd;
		int y;
		int dy;
	};
	std::stack<Seed, std::vector<Seed>> seedStack;
	seedStack.push({ floor.x, floor.x + 1, floor.y, 1 });

	const auto isInside = [floorID](int x, int y) {
		if (dTransVal[x][y] != 0)
			return false;
		return IsFloor({ x, y }, floorID);
	};

	const auto set = [floorID](int x, int y) {
		FillTransparencyValues({ x, y }, floorID);
	};

	const Displacement left = { -1, 0 };
	const Displacement right = { 1, 0 };
	const auto checkDiagonals = [&](Point p, Displacement direction) {
		const Point up = p + Displacement { 0, -1 };
		const Point upOver = up + direction;
		if (!isInside(up.x, up.y) && isInside(upOver.x, upOver.y))
			seedStack.push({ upOver.x, upOver.x + 1, upOver.y, -1 });

		const Point down = p + Displacement { 0, 1 };
		const Point downOver = down + direction;
		if (!isInside(down.x, down.y) && isInside(downOver.x, downOver.y))
			seedStack.push(Seed { downOver.x, downOver.x + 1, downOver.y, 1 });
	};

	while (!seedStack.empty()) {
		const auto [scanStart, scanEnd, y, dy] = seedStack.top();
		seedStack.pop();

		int scanLeft = scanStart;
		if (isInside(scanLeft, y)) {
			while (isInside(scanLeft - 1, y)) {
				set(scanLeft - 1, y);
				scanLeft--;
			}
			checkDiagonals({ scanLeft, y }, left);
		}
		if (scanLeft < scanStart)
			seedStack.push(Seed { scanLeft, scanStart - 1, y - dy, -dy });

		int scanRight = scanStart;
		while (scanRight < scanEnd) {
			while (isInside(scanRight, y)) {
				set(scanRight, y);
				scanRight++;
			}
			seedStack.push(Seed { scanLeft, scanRight - 1, y + dy, dy });
			if (scanRight - 1 > scanEnd)
				seedStack.push(Seed { scanEnd + 1, scanRight - 1, y - dy, -dy });
			if (scanLeft < scanRight)
				checkDiagonals({ scanRight - 1, y }, right);

			while (scanRight < scanEnd && !isInside(scanRight, y))
				scanRight++;
			scanLeft = scanRight;
			if (scanLeft < scanEnd)
				checkDiagonals({ scanLeft, y }, left);
		}
	}
}

void InitGlobals()
{
	memset(dFlags, 0, sizeof(dFlags));
	memset(dPlayer, 0, sizeof(dPlayer));
	memset(dMonster, 0, sizeof(dMonster));
	memset(dCorpse, 0, sizeof(dCorpse));
	memset(dItem, 0, sizeof(dItem));
	memset(dObject, 0, sizeof(dObject));
	memset(dSpecial, 0, sizeof(dSpecial));
	uint8_t defaultLight = leveltype == DTYPE_TOWN ? 0 : 15;
#ifdef _DEBUG
	if (DisableLighting)
		defaultLight = 0;
#endif
	memset(dLight, defaultLight, sizeof(dLight));

	DRLG_InitTrans();

	dminPosition = WorldTilePosition(0, 0).megaToWorld();
	dmaxPosition = WorldTilePosition(40, 40).megaToWorld();
	SetPieceRoom = { { 0, 0 }, { 0, 0 } };
	SetPiece = { { 0, 0 }, { 0, 0 } };
}

} // namespace

#ifdef BUILD_TESTING
std::optional<WorldTileSize> GetSizeForThemeRoom()
{
	return GetSizeForThemeRoom(0, { 0, 0 }, 5, 10);
}
#endif

dungeon_type GetLevelType(int level)
{
	if (level == 0)
		return DTYPE_TOWN;
	if (level <= 4)
		return DTYPE_CATHEDRAL;
	if (level <= 8)
		return DTYPE_CATACOMBS;
	if (level <= 12)
		return DTYPE_CAVES;
	if (level <= 16)
		return DTYPE_HELL;
	if (level <= 20)
		return DTYPE_NEST;
	if (level <= 24)
		return DTYPE_CRYPT;

	return DTYPE_NONE;
}

void CreateDungeon(uint32_t rseed, lvl_entry entry)
{
	InitGlobals();

	switch (leveltype) {
	case DTYPE_TOWN:
		CreateTown(entry);
		break;
	case DTYPE_CATHEDRAL:
	case DTYPE_CRYPT:
		CreateL5Dungeon(rseed, entry);
		break;
	case DTYPE_CATACOMBS:
		CreateL2Dungeon(rseed, entry);
		break;
	case DTYPE_CAVES:
	case DTYPE_NEST:
		CreateL3Dungeon(rseed, entry);
		break;
	case DTYPE_HELL:
		CreateL4Dungeon(rseed, entry);
		break;
	default:
		app_fatal("Invalid level type");
	}

	Make_SetPC(SetPiece);
}

void DRLG_InitTrans()
{
	memset(dTransVal, 0, sizeof(dTransVal));
	TransList = {}; // TODO duplicate reset in InitLighting()
	TransVal = 1;
}

void DRLG_RectTrans(WorldTileRectangle area)
{
	const WorldTilePosition position = area.position;
	const WorldTileSize size = area.size;

	for (int j = position.y; j <= position.y + size.height; j++) {
		for (int i = position.x; i <= position.x + size.width; i++) {
			dTransVal[i][j] = TransVal;
		}
	}

	TransVal++;
}

void DRLG_MRectTrans(WorldTileRectangle area)
{
	DRLG_RectTrans({ area.position.megaToWorld() + WorldTileDisplacement { 1, 1 }, area.size * 2 - 1 });
}

void DRLG_MRectTrans(WorldTilePosition origin, WorldTilePosition extent)
{
	DRLG_MRectTrans({ origin, WorldTileSize(extent.x - origin.x, extent.y - origin.y) });
}

void DRLG_CopyTrans(int sx, int sy, int dx, int dy)
{
	dTransVal[dx][dy] = dTransVal[sx][sy];
}

void LoadTransparency(const uint16_t *dunData)
{
	WorldTileSize size = GetDunSize(dunData);

	const int layer2Offset = 2 + (size.width * size.height);

	// The rest of the layers are at dPiece scale
	size *= static_cast<WorldTileCoord>(2);

	const uint16_t *transparentLayer = &dunData[layer2Offset + (size.width * size.height * 3)];

	for (WorldTileCoord j = 0; j < size.height; j++) {
		for (WorldTileCoord i = 0; i < size.width; i++) {
			dTransVal[16 + i][16 + j] = static_cast<int8_t>(Swap16LE(*transparentLayer));
			transparentLayer++;
		}
	}
}

void LoadDungeonBase(const char *path, Point spawn, int floorId, int dirtId)
{
	ViewPosition = spawn;

	InitGlobals();

	memset(dungeon, dirtId, sizeof(dungeon));

	auto dunData = LoadFileInMem<uint16_t>(path);
	PlaceDunTiles(dunData.get(), { 0, 0 }, floorId);
	LoadTransparency(dunData.get());

	SetMapMonsters(dunData.get(), Point(0, 0).megaToWorld());
	InitAllMonsterGFX();
	SetMapObjects(dunData.get(), 0, 0);
}

void Make_SetPC(WorldTileRectangle area)
{
	const WorldTilePosition position = area.position.megaToWorld();
	const WorldTileSize size = area.size * 2;

	for (unsigned j = 0; j < size.height; j++) {
		for (unsigned i = 0; i < size.width; i++) {
			dFlags[position.x + i][position.y + j] |= DungeonFlag::Populated;
		}
	}
}

std::optional<Point> PlaceMiniSet(const Miniset &miniset, int tries, bool drlg1Quirk)
{
	const int sw = miniset.size.width;
	const int sh = miniset.size.height;
	Point position { GenerateRnd(DMAXX - sw), GenerateRnd(DMAXY - sh) };

	for (int i = 0; i < tries; i++, position.x++) {
		if (position.x == DMAXX - sw) {
			position.x = 0;
			position.y++;
			if (position.y == DMAXY - sh) {
				position.y = 0;
			}
		}

		// Limit the position of SetPieces for compatibility with Diablo bug
		if (drlg1Quirk) {
			bool valid = true;
			if (position.x <= 12) {
				position.x++;
				valid = false;
			}
			if (position.y <= 12) {
				position.y++;
				valid = false;
			}
			if (!valid) {
				continue;
			}
		}

		if (SetPieceRoom.contains(position))
			continue;
		if (!miniset.matches(position))
			continue;

		miniset.place(position);

		return position;
	}

	return {};
}

void PlaceDunTiles(const uint16_t *dunData, Point position, int floorId)
{
	const WorldTileSize size = GetDunSize(dunData);

	const uint16_t *tileLayer = &dunData[2];

	for (WorldTileCoord j = 0; j < size.height; j++) {
		for (WorldTileCoord i = 0; i < size.width; i++) {
			auto tileId = static_cast<uint8_t>(Swap16LE(tileLayer[(j * size.width) + i]));
			if (tileId != 0) {
				dungeon[position.x + i][position.y + j] = tileId;
				Protected.set(position.x + i, position.y + j);
			} else if (floorId != 0) {
				dungeon[position.x + i][position.y + j] = floorId;
			}
		}
	}
}

void DRLG_PlaceThemeRooms(int minSize, int maxSize, int floor, int freq, bool rndSize)
{
	themeCount = 0;
	memset(themeLoc, 0, sizeof(*themeLoc));
	for (WorldTileCoord j = 0; j < DMAXY; j++) {
		for (WorldTileCoord i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == floor && FlipCoin(freq)) {
				std::optional<WorldTileSize> themeSize = GetSizeForThemeRoom(floor, { i, j }, minSize, maxSize);

				if (!themeSize)
					continue;

				if (rndSize) {
					const int min = minSize - 2;
					const int max = maxSize - 2;
					themeSize->width = min + GenerateRnd(GenerateRnd(themeSize->width - min + 1));
					if (themeSize->width < min || themeSize->width > max)
						themeSize->width = min;
					themeSize->height = min + GenerateRnd(GenerateRnd(themeSize->height - min + 1));
					if (themeSize->height < min || themeSize->height > max)
						themeSize->height = min;
				}

				THEME_LOC &theme = themeLoc[themeCount];
				theme.room = { WorldTilePosition { i, j } + Direction::South, *themeSize };
				if (IsAnyOf(leveltype, DTYPE_CAVES, DTYPE_NEST)) {
					DRLG_RectTrans({ (theme.room.position + Direction::South).megaToWorld(), theme.room.size * 2 - 5 });
				} else {
					DRLG_MRectTrans({ theme.room.position, theme.room.size - 1 });
				}
				theme.ttval = TransVal - 1;
				CreateThemeRoom(themeCount);
				themeCount++;
			}
		}
	}
} // namespace

void DRLG_HoldThemeRooms()
{
	for (int i = 0; i < themeCount; i++) {
		for (int y = themeLoc[i].room.position.y; y < themeLoc[i].room.position.y + themeLoc[i].room.size.height - 1; y++) {
			for (int x = themeLoc[i].room.position.x; x < themeLoc[i].room.position.x + themeLoc[i].room.size.width - 1; x++) {
				const int xx = (2 * x) + 16;
				const int yy = (2 * y) + 16;
				dFlags[xx][yy] |= DungeonFlag::Populated;
				dFlags[xx + 1][yy] |= DungeonFlag::Populated;
				dFlags[xx][yy + 1] |= DungeonFlag::Populated;
				dFlags[xx + 1][yy + 1] |= DungeonFlag::Populated;
			}
		}
	}
}

WorldTileSize GetDunSize(const uint16_t *dunData)
{
	return WorldTileSize(static_cast<WorldTileCoord>(Swap16LE(dunData[0])), static_cast<WorldTileCoord>(Swap16LE(dunData[1])));
}

void DRLG_LPass3(int lv)
{
	{
		const MegaTile mega = pMegaTiles[lv];
		const int v1 = Swap16LE(mega.micro1);
		const int v2 = Swap16LE(mega.micro2);
		const int v3 = Swap16LE(mega.micro3);
		const int v4 = Swap16LE(mega.micro4);

		for (int j = 0; j < MAXDUNY; j += 2) {
			for (int i = 0; i < MAXDUNX; i += 2) {
				dPiece[i + 0][j + 0] = v1;
				dPiece[i + 1][j + 0] = v2;
				dPiece[i + 0][j + 1] = v3;
				dPiece[i + 1][j + 1] = v4;
			}
		}
	}

	int yy = 16;
	for (int j = 0; j < DMAXY; j++) {
		int xx = 16;
		for (int i = 0; i < DMAXX; i++) { // NOLINT(modernize-loop-convert)
			const int tileId = dungeon[i][j] - 1;
			const MegaTile mega = pMegaTiles[tileId];
			dPiece[xx + 0][yy + 0] = Swap16LE(mega.micro1);
			dPiece[xx + 1][yy + 0] = Swap16LE(mega.micro2);
			dPiece[xx + 0][yy + 1] = Swap16LE(mega.micro3);
			dPiece[xx + 1][yy + 1] = Swap16LE(mega.micro4);
			xx += 2;
		}
		yy += 2;
	}
}

bool IsNearThemeRoom(WorldTilePosition testPosition)
{
	for (int i = 0; i < themeCount; i++) {
		if (WorldTileRectangle(themeLoc[i].room.position - WorldTileDisplacement { 2 }, themeLoc[i].room.size + 5).contains(testPosition))
			return true;
	}

	return false;
}

void InitLevels()
{
	currlevel = 0;
	leveltype = DTYPE_TOWN;
	setlevel = false;
}

void FloodTransparencyValues(uint8_t floorID)
{
	int yy = 16;
	for (int j = 0; j < DMAXY; j++) {
		int xx = 16;
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == floorID && dTransVal[xx][yy] == 0) {
				FindTransparencyValues({ xx, yy }, floorID);
				TransVal++;
			}
			xx += 2;
		}
		yy += 2;
	}
}

} // namespace devilution
