/**
 * @file themes.cpp
 *
 * Implementation of the theme room placing algorithms.
 */
#include "levels/themes.h"

#include <cstdint>

#include <fmt/core.h>

#include "engine/path.h"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "items.h"
#include "levels/tile_properties.hpp"
#include "levels/trigs.h"
#include "monster.h"
#include "objects.h"
#include "quests.h"
#include "utils/algorithm/container.hpp"
#include "utils/is_of.hpp"
#include "utils/str_cat.hpp"

namespace devilution {

int numthemes;
bool armorFlag;
bool weaponFlag;
int zharlib;
ThemeStruct themes[MAXTHEMES];

namespace {

bool cauldronFlag;
bool bFountainFlag;
bool mFountainFlag;
bool pFountainFlag;
bool tFountainFlag;
bool treasureFlag;

int themex;
int themey;
size_t themeVar1;

bool TFit_Shrine(int i)
{
	Point position { 0, 0 };
	size_t found = 0;

	while (found == 0) {
		const Point testPosition = position;
		if (dTransVal[position.x][position.y] == themes[i].ttval) {
			if (TileHasAny(position + Direction::NorthEast, TileProperties::Trap)
			    && IsTileNotSolid(testPosition + Direction::NorthWest)
			    && IsTileNotSolid(testPosition + Direction::SouthEast)
			    && dTransVal[position.x - 1][position.y] == themes[i].ttval
			    && dTransVal[position.x + 1][position.y] == themes[i].ttval
			    && !IsObjectAtPosition(testPosition + Direction::North)
			    && !IsObjectAtPosition(testPosition + Direction::East)) {
				found = 1;
			}
			if (found == 0
			    && TileHasAny(position + Direction::NorthWest, TileProperties::Trap)
			    && IsTileNotSolid(testPosition + Direction::NorthEast)
			    && IsTileNotSolid(testPosition + Direction::SouthWest)
			    && dTransVal[position.x][position.y - 1] == themes[i].ttval
			    && dTransVal[position.x][position.y + 1] == themes[i].ttval
			    && !IsObjectAtPosition(testPosition + Direction::North)
			    && !IsObjectAtPosition(testPosition + Direction::West)) {
				found = 2;
			}
		}
		if (found == 0) {
			position.x++;
			if (position.x == MAXDUNX) {
				position.x = 0;
				position.y++;
				if (position.y == MAXDUNY)
					return false;
			}
		}
	}
	themex = position.x;
	themey = position.y;
	themeVar1 = found;
	return true;
}

bool CheckThemeObj5(Point origin, int8_t regionId)
{
	return c_all_of(PointsInRectangle(Rectangle { origin, 2 }), [regionId](Point testPosition) {
		// note out-of-bounds tiles are not solid, this function relies on the guard in TFit_Obj5 and dungeon border
		if (IsTileSolid(testPosition)) {
			return false;
		}
		// If the theme object would extend into a different region then it doesn't fit.
		if (dTransVal[testPosition.x][testPosition.y] != regionId) {
			return false;
		}
		return true;
	});
}

bool TFit_Obj5(int t)
{
	const int targetCandidates = GenerateRnd(5);
	if (targetCandidates < 0) {
		// vanilla rng can return -3 for GenerateRnd(5), default behaviour is to set the output to 0,0 and return true in this case...
		themex = 0;
		themey = 0;
		return true;
	}

	int candidatesFound = 0;
	for (const Point tile : PointsInRectangle(Rectangle { { 0, 0 }, { MAXDUNX, MAXDUNY } })) {
		if (dTransVal[tile.x][tile.y] == themes[t].ttval && IsTileNotSolid(tile) && CheckThemeObj5(tile, themes[t].ttval)) {
			// Use themex/y to keep track of the last candidate area found, in case we end up with fewer candidates than the target
			themex = tile.x;
			themey = tile.y;
			candidatesFound++;
			if (candidatesFound > targetCandidates)
				return true;
		}
	}
	return candidatesFound > 0;
}
bool TFit_SkelRoom(int t)
{
	if (IsNoneOf(leveltype, DTYPE_CATHEDRAL, DTYPE_CATACOMBS)) {
		return false;
	}

	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (IsSkel(LevelMonsterTypes[i].type)) {
			themeVar1 = i;
			return TFit_Obj5(t);
		}
	}

	return false;
}

bool TFit_GoatShrine(int t)
{
	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (IsGoat(LevelMonsterTypes[i].type)) {
			themeVar1 = i;
			return TFit_Obj5(t);
		}
	}

	return false;
}

bool CheckThemeObj3(Point origin, int8_t regionId, unsigned frequency = 0)
{
	return c_all_of(PointsInRectangle(Rectangle { origin, 1 }), [regionId, frequency](Point testPosition) {
		if (!InDungeonBounds(testPosition)) {
			return false;
		}
		if (IsTileSolid(testPosition)) {
			return false;
		}
		// If the theme object would extend into a different region then it doesn't fit.
		if (dTransVal[testPosition.x][testPosition.y] != regionId) {
			return false;
		}
		if (IsObjectAtPosition(testPosition)) {
			return false;
		}
		if (frequency > 0 && FlipCoin(frequency)) {
			return false;
		}
		return true;
	});
}

bool TFit_Obj3(int8_t regionId)
{
	constexpr unsigned objrnd[4] = { 4, 4, 3, 5 };

	for (int yp = 1; yp < MAXDUNY - 1; yp++) {
		for (int xp = 1; xp < MAXDUNX - 1; xp++) {
			if (CheckThemeObj3({ xp, yp }, regionId, objrnd[leveltype - 1])) {
				themex = xp;
				themey = yp;
				return true;
			}
		}
	}

	return false;
}

bool CheckThemeReqs(theme_id t)
{
	switch (t) {
	case THEME_SHRINE:
	case THEME_SKELROOM:
	case THEME_LIBRARY:
		return IsNoneOf(leveltype, DTYPE_CAVES, DTYPE_HELL);
	case THEME_ARMORSTAND:
	case THEME_WEAPONRACK:
		return IsNoneOf(leveltype, DTYPE_CATHEDRAL);
	case THEME_CAULDRON:
		return leveltype == DTYPE_HELL && cauldronFlag;
	case THEME_BLOODFOUNTAIN:
		return bFountainFlag;
	case THEME_PURIFYINGFOUNTAIN:
		return pFountainFlag;
	case THEME_MURKYFOUNTAIN:
		return mFountainFlag;
	case THEME_TEARFOUNTAIN:
		return tFountainFlag;
	case THEME_TREASURE:
		return treasureFlag;
	default:
		return true;
	}
}

bool SpecialThemeFit(int i, theme_id t)
{
	bool rv = CheckThemeReqs(t);
	switch (t) {
	case THEME_SHRINE:
	case THEME_LIBRARY:
		if (rv) {
			rv = TFit_Shrine(i);
		}
		break;
	case THEME_SKELROOM:
		if (rv) {
			rv = TFit_SkelRoom(i);
		}
		break;
	case THEME_BLOODFOUNTAIN:
		if (rv) {
			rv = TFit_Obj5(i);
		}
		if (rv) {
			bFountainFlag = false;
		}
		break;
	case THEME_PURIFYINGFOUNTAIN:
		if (rv) {
			rv = TFit_Obj5(i);
		}
		if (rv) {
			pFountainFlag = false;
		}
		break;
	case THEME_MURKYFOUNTAIN:
		if (rv) {
			rv = TFit_Obj5(i);
		}
		if (rv) {
			mFountainFlag = false;
		}
		break;
	case THEME_TEARFOUNTAIN:
		if (rv) {
			rv = TFit_Obj5(i);
		}
		if (rv) {
			tFountainFlag = false;
		}
		break;
	case THEME_CAULDRON:
		if (rv) {
			rv = TFit_Obj5(i);
		}
		if (rv) {
			cauldronFlag = false;
		}
		break;
	case THEME_GOATSHRINE:
		if (rv) {
			rv = TFit_GoatShrine(i);
		}
		break;
	case THEME_TORTURE:
	case THEME_DECAPITATED:
	case THEME_ARMORSTAND:
	case THEME_BRNCROSS:
	case THEME_WEAPONRACK:
		if (rv) {
			rv = TFit_Obj3(themes[i].ttval);
		}
		break;
	case THEME_TREASURE:
		if (rv) {
			treasureFlag = false;
		}
		break;
	default:
		break;
	}

	return rv;
}

bool CheckThemeRoom(int8_t tv)
{
	for (int i = 0; i < numtrigs; i++) {
		if (dTransVal[trigs[i].position.x][trigs[i].position.y] == tv)
			return false;
	}

	int tarea = 0;
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) {
			if (dTransVal[i][j] != tv)
				continue;
			if (TileContainsSetPiece({ i, j }))
				return false;

			tarea++;
		}
	}

	if (leveltype == DTYPE_CATHEDRAL && (tarea < 9 || tarea > 100))
		return false;

	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) {
			if (dTransVal[i][j] != tv || TileHasAny({ i, j }, TileProperties::Solid))
				continue;
			if (dTransVal[i - 1][j] != tv && IsTileNotSolid({ i - 1, j }))
				return false;
			if (dTransVal[i + 1][j] != tv && IsTileNotSolid({ i + 1, j }))
				return false;
			if (dTransVal[i][j - 1] != tv && IsTileNotSolid({ i, j - 1 }))
				return false;
			if (dTransVal[i][j + 1] != tv && IsTileNotSolid({ i, j + 1 }))
				return false;
		}
	}

	return true;
}

/**
 * PlaceThemeMonsts places theme monsters with the specified frequency.
 *
 * @param t theme number (index into themes array).
 * @param f frequency (1/f likelihood of adding monster).
 */
void PlaceThemeMonsts(int t, int f)
{
	size_t scattertypes[138];

	int numscattypes = 0;
	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if ((LevelMonsterTypes[i].placeFlags & PLACE_SCATTER) != 0) {
			scattertypes[numscattypes] = i;
			numscattypes++;
		}
	}
	const size_t mtype = scattertypes[GenerateRnd(numscattypes)];
	for (int yp = 0; yp < MAXDUNY; yp++) {
		for (int xp = 0; xp < MAXDUNX; xp++) {
			if (dTransVal[xp][yp] == themes[t].ttval && IsTileNotSolid({ xp, yp }) && dItem[xp][yp] == 0 && !IsObjectAtPosition({ xp, yp })) {
				if (FlipCoin(f)) {
					AddMonster({ xp, yp }, static_cast<Direction>(GenerateRnd(8)), mtype, true);
				}
			}
		}
	}
}

/**
 * Theme_Barrel initializes the barrel theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_Barrel(int t)
{
	const int barrnd[4] = { 2, 6, 4, 8 };
	const int monstrnd[4] = { 5, 7, 3, 9 };

	for (int yp = 0; yp < MAXDUNY; yp++) {
		for (int xp = 0; xp < MAXDUNX; xp++) {
			if (dTransVal[xp][yp] == themes[t].ttval && IsTileNotSolid({ xp, yp })) {
				if (FlipCoin(barrnd[leveltype - 1])) {
					const _object_id r = FlipCoin(barrnd[leveltype - 1]) ? OBJ_BARREL : OBJ_BARRELEX;
					AddObject(r, { xp, yp });
				}
			}
		}
	}
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_Shrine initializes the shrine theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_Shrine(int t)
{
	const int monstrnd[4] = { 6, 6, 3, 9 };

	TFit_Shrine(t);
	if (themeVar1 == 1) {
		AddObject(OBJ_CANDLE2, { themex - 1, themey });
		AddObject(OBJ_SHRINER, { themex, themey });
		AddObject(OBJ_CANDLE2, { themex + 1, themey });
	} else {
		AddObject(OBJ_CANDLE2, { themex, themey - 1 });
		AddObject(OBJ_SHRINEL, { themex, themey });
		AddObject(OBJ_CANDLE2, { themex, themey + 1 });
	}
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_MonstPit initializes the monster pit theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_MonstPit(int t)
{
	const int monstrnd[4] = { 6, 7, 3, 9 };

	int r = GenerateRnd(100) + 1;
	int ixp = 0;
	int iyp = 0;
	while (r > 0) {
		if (dTransVal[ixp][iyp] == themes[t].ttval && IsTileNotSolid({ ixp, iyp })) {
			--r;
		}
		if (r <= 0)
			continue;
		ixp++;
		if (ixp == MAXDUNX) {
			ixp = 0;
			iyp++;
			if (iyp == MAXDUNY) {
				iyp = 0;
			}
		}
	}
	CreateRndItem({ ixp, iyp }, true, false, true);
	ItemNoFlippy();
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

void SpawnObjectOrSkeleton(unsigned frequency, _object_id objectType, Point tile)
{
	if (FlipCoin(frequency)) {
		AddObject(objectType, tile);
	} else {
		Monster *skeleton = PreSpawnSkeleton();
		if (skeleton != nullptr)
			ActivateSkeleton(*skeleton, tile);
	}
}

/**
 * Theme_SkelRoom initializes the skeleton room theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_SkelRoom(int t)
{
	constexpr unsigned monstrnd[4] = { 6, 7, 3, 9 };

	TFit_SkelRoom(t);

	const int xp = themex;
	const int yp = themey;

	AddObject(OBJ_SKFIRE, { xp, yp });

	SpawnObjectOrSkeleton(monstrnd[leveltype - 1], OBJ_BANNERL, { xp - 1, yp - 1 });

	{
		Monster *skeleton = PreSpawnSkeleton();
		if (skeleton != nullptr)
			ActivateSkeleton(*skeleton, { xp, yp - 1 });
	}

	SpawnObjectOrSkeleton(monstrnd[leveltype - 1], OBJ_BANNERR, { xp + 1, yp - 1 });

	SpawnObjectOrSkeleton(monstrnd[leveltype - 1], OBJ_BANNERM, { xp - 1, yp });

	SpawnObjectOrSkeleton(monstrnd[leveltype - 1], OBJ_BANNERM, { xp + 1, yp });

	SpawnObjectOrSkeleton(monstrnd[leveltype - 1], OBJ_BANNERR, { xp - 1, yp + 1 });

	{
		Monster *skeleton = PreSpawnSkeleton();
		if (skeleton != nullptr)
			ActivateSkeleton(*skeleton, { xp, yp + 1 });
	}

	SpawnObjectOrSkeleton(monstrnd[leveltype - 1], OBJ_BANNERL, { xp + 1, yp + 1 });

	if (!IsObjectAtPosition({ xp, yp - 3 })) {
		AddObject(OBJ_SKELBOOK, { xp, yp - 2 });
	}
	if (!IsObjectAtPosition({ xp, yp + 3 })) {
		AddObject(OBJ_SKELBOOK, { xp, yp + 2 });
	}
}

/**
 * Theme_Treasure initializes the treasure theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_Treasure(int t)
{
	const int treasrnd[4] = { 4, 9, 7, 10 };
	const int monstrnd[4] = { 6, 8, 3, 7 };

	DiscardRandomValues(1);
	for (int yp = 0; yp < MAXDUNY; yp++) {
		for (int xp = 0; xp < MAXDUNX; xp++) {
			if (dTransVal[xp][yp] == themes[t].ttval && IsTileNotSolid({ xp, yp })) {
				const int8_t treasureType = treasrnd[leveltype - 1];
				const int rv = GenerateRnd(treasureType);
				// BUGFIX: this used to be `2*GenerateRnd(treasureType) == 0` however 2*0 has no effect, should probably be `FlipCoin(2*treasureType)`
				if (FlipCoin(treasureType)) {
					CreateTypeItem({ xp, yp }, false, ItemType::Gold, IMISC_NONE, false, true);
					ItemNoFlippy();
				}
				if (rv == 0) {
					CreateRndItem({ xp, yp }, false, false, true);
					ItemNoFlippy();
				}
				// BUGFIX: the following code is likely not working as intended.
				//    `rv >= treasureType - 2` is not connected to either
				//    of the item creation branches above, thus the last (unrelated)
				//    item spawned/dropped on ground would be halved in value.
				if (rv >= treasureType - 2 && leveltype != DTYPE_CATHEDRAL) {
					Item &item = Items[ActiveItems[ActiveItemCount - 1]];
					if (item.IDidx == IDI_GOLD) {
						item._ivalue = std::max(item._ivalue / 2, 1);
					}
				}
			}
		}
	}
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_Library initializes the library theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_Library(int t)
{
	constexpr unsigned librnd[4] = { 1, 2, 2, 5 };
	const int monstrnd[4] = { 5, 7, 3, 9 };

	TFit_Shrine(t);

	if (themeVar1 == 1) {
		AddObject(OBJ_BOOKCANDLE, { themex - 1, themey });
		AddObject(OBJ_BOOKCASER, { themex, themey });
		AddObject(OBJ_BOOKCANDLE, { themex + 1, themey });
	} else {
		AddObject(OBJ_BOOKCANDLE, { themex, themey - 1 });
		AddObject(OBJ_BOOKCASEL, { themex, themey });
		AddObject(OBJ_BOOKCANDLE, { themex, themey + 1 });
	}

	for (int yp = 1; yp < MAXDUNY - 1; yp++) {
		for (int xp = 1; xp < MAXDUNX - 1; xp++) {
			if (CheckThemeObj3({ xp, yp }, themes[t].ttval) && dMonster[xp][yp] == 0 && FlipCoin(librnd[leveltype - 1])) {
				Object *bookstand = AddObject(OBJ_BOOKSTAND, { xp, yp });
				if (!FlipCoin(2 * librnd[leveltype - 1])) {
					if (bookstand != nullptr) {
						bookstand->selectionRegion = SelectionRegion::None;
						bookstand->_oAnimFrame += 2;
					}
				}
			}
		}
	}

	if (Quests[Q_ZHAR].IsAvailable() && t == zharlib) {
		return;
	}
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_Torture initializes the torture theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_Torture(int t)
{
	constexpr unsigned tortrnd[4] = { 6, 8, 3, 8 };
	const int monstrnd[4] = { 6, 8, 3, 9 };

	for (int yp = 1; yp < MAXDUNY - 1; yp++) {
		for (int xp = 1; xp < MAXDUNX - 1; xp++) {
			if (dTransVal[xp][yp] == themes[t].ttval && IsTileNotSolid({ xp, yp })) {
				if (CheckThemeObj3({ xp, yp }, themes[t].ttval)) {
					if (FlipCoin(tortrnd[leveltype - 1])) {
						AddObject(OBJ_TNUDEM2, { xp, yp });
					}
				}
			}
		}
	}
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_BloodFountain initializes the blood fountain theme.
 * @param t Theme number (index into themes array).
 */
void Theme_BloodFountain(int t)
{
	const int monstrnd[4] = { 6, 8, 3, 9 };

	TFit_Obj5(t);
	AddObject(OBJ_BLOODFTN, { themex, themey });
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_Decap initializes the decapitated theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_Decap(int t)
{
	constexpr unsigned decaprnd[4] = { 6, 8, 3, 8 };
	const int monstrnd[4] = { 6, 8, 3, 9 };

	for (int yp = 1; yp < MAXDUNY - 1; yp++) {
		for (int xp = 1; xp < MAXDUNX - 1; xp++) {
			if (dTransVal[xp][yp] == themes[t].ttval && IsTileNotSolid({ xp, yp })) {
				if (CheckThemeObj3({ xp, yp }, themes[t].ttval)) {
					if (FlipCoin(decaprnd[leveltype - 1])) {
						AddObject(OBJ_DECAP, { xp, yp });
					}
				}
			}
		}
	}
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_PurifyingFountain initializes the purifying fountain theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_PurifyingFountain(int t)
{
	const int monstrnd[4] = { 6, 7, 3, 9 };

	TFit_Obj5(t);
	AddObject(OBJ_PURIFYINGFTN, { themex, themey });
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_ArmorStand initializes the armor stand theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_ArmorStand(int t)
{
	constexpr unsigned armorrnd[4] = { 6, 8, 3, 8 };
	const int monstrnd[4] = { 6, 7, 3, 9 };

	if (armorFlag) {
		TFit_Obj3(themes[t].ttval);
		AddObject(OBJ_ARMORSTAND, { themex, themey });
	}
	for (int yp = 0; yp < MAXDUNY; yp++) {
		for (int xp = 0; xp < MAXDUNX; xp++) {
			if (dTransVal[xp][yp] == themes[t].ttval && IsTileNotSolid({ xp, yp })) {
				if (CheckThemeObj3({ xp, yp }, themes[t].ttval)) {
					if (FlipCoin(armorrnd[leveltype - 1])) {
						AddObject(OBJ_ARMORSTANDN, { xp, yp });
					}
				}
			}
		}
	}
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
	armorFlag = false;
}

/**
 * Theme_GoatShrine initializes the goat shrine theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_GoatShrine(int t)
{
	TFit_GoatShrine(t);
	AddObject(OBJ_GOATSHRINE, { themex, themey });
	for (int yy = themey - 1; yy <= themey + 1; yy++) {
		for (int xx = themex - 1; xx <= themex + 1; xx++) {
			if (dTransVal[xx][yy] == themes[t].ttval && IsTileNotSolid({ xx, yy }) && (xx != themex || yy != themey)) {
				AddMonster({ xx, yy }, Direction::SouthWest, themeVar1, true);
			}
		}
	}
}

/**
 * Theme_Cauldron initializes the cauldron theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_Cauldron(int t)
{
	const int monstrnd[4] = { 6, 7, 3, 9 };

	TFit_Obj5(t);
	AddObject(OBJ_CAULDRON, { themex, themey });
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_MurkyFountain initializes the murky fountain theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_MurkyFountain(int t)
{
	const int monstrnd[4] = { 6, 7, 3, 9 };

	TFit_Obj5(t);
	AddObject(OBJ_MURKYFTN, { themex, themey });
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_TearFountain initializes the tear fountain theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_TearFountain(int t)
{
	const int monstrnd[4] = { 6, 7, 3, 9 };

	TFit_Obj5(t);
	AddObject(OBJ_TEARFTN, { themex, themey });
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_BrnCross initializes the burning cross theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_BrnCross(int t)
{
	const int8_t regionId = themes[t].ttval;
	const int monstrnd[4] = { 6, 8, 3, 9 };
	constexpr unsigned bcrossrnd[4] = { 5, 7, 3, 8 };

	for (int yp = 0; yp < MAXDUNY; yp++) {
		for (int xp = 0; xp < MAXDUNX; xp++) {
			if (dTransVal[xp][yp] == regionId && IsTileNotSolid({ xp, yp })) {
				if (CheckThemeObj3({ xp, yp }, regionId)) {
					if (FlipCoin(bcrossrnd[leveltype - 1])) {
						AddObject(OBJ_TBCROSS, { xp, yp });
					}
				}
			}
		}
	}
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
}

/**
 * Theme_WeaponRack initializes the weapon rack theme.
 *
 * @param t theme number (index into themes array).
 */
void Theme_WeaponRack(int t)
{
	const int8_t regionId = themes[t].ttval;
	constexpr unsigned weaponrnd[4] = { 6, 8, 5, 8 };
	const int monstrnd[4] = { 6, 7, 3, 9 };

	if (weaponFlag) {
		TFit_Obj3(regionId);
		AddObject(OBJ_WEAPONRACK, { themex, themey });
	}
	for (int yp = 0; yp < MAXDUNY; yp++) {
		for (int xp = 0; xp < MAXDUNX; xp++) {
			if (dTransVal[xp][yp] == regionId && IsTileNotSolid({ xp, yp })) {
				if (CheckThemeObj3({ xp, yp }, regionId)) {
					if (FlipCoin(weaponrnd[leveltype - 1])) {
						AddObject(OBJ_WEAPONRACKN, { xp, yp });
					}
				}
			}
		}
	}
	PlaceThemeMonsts(t, monstrnd[leveltype - 1]);
	weaponFlag = false;
}

/**
 * UpdateL4Trans sets each value of the transparency map to 1.
 */
void UpdateL4Trans()
{
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) { // NOLINT(modernize-loop-convert)
			if (dTransVal[i][j] != 0) {
				dTransVal[i][j] = 1;
			}
		}
	}
}

} // namespace

void InitThemes()
{
	zharlib = -1;
	numthemes = 0;
	armorFlag = true;
	bFountainFlag = true;
	cauldronFlag = true;
	mFountainFlag = true;
	pFountainFlag = true;
	tFountainFlag = true;
	treasureFlag = true;
	weaponFlag = true;

	if (currlevel == 16 || IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
		return;
	}

	/** Specifies the set of special theme IDs from which one will be selected at random. */
	constexpr theme_id ThemeGood[4] = { THEME_GOATSHRINE, THEME_SHRINE, THEME_SKELROOM, THEME_LIBRARY };

	if (leveltype == DTYPE_CATHEDRAL) {
		for (int8_t i = 0; numthemes < MAXTHEMES; i++) {
			if (CheckThemeRoom(i)) {
				themes[numthemes].ttval = i;
				theme_id j = ThemeGood[GenerateRnd(4)];
				while (!SpecialThemeFit(numthemes, j)) {
					j = (theme_id)GenerateRnd(17);
				}
				themes[numthemes].ttype = j;
				numthemes++;
			}
			if (i > TransVal)
				break;
		}
		return;
	}

	for (int i = 0; i < themeCount; i++) {
		themes[i].ttype = THEME_NONE;
	}

	if (Quests[Q_ZHAR].IsAvailable()) {
		for (int j = 0; j < themeCount; j++) {
			themes[j].ttval = themeLoc[j].ttval;
			if (SpecialThemeFit(j, THEME_LIBRARY)) {
				themes[j].ttype = THEME_LIBRARY;
				zharlib = j;
				break;
			}
		}
	}
	for (int i = 0; i < themeCount; i++) {
		if (themes[i].ttype == THEME_NONE) {
			themes[i].ttval = themeLoc[i].ttval;
			theme_id j = ThemeGood[GenerateRnd(4)];
			while (!SpecialThemeFit(i, j)) {
				j = (theme_id)GenerateRnd(17);
			}
			themes[i].ttype = j;
		}
	}
	numthemes += themeCount;
}

void HoldThemeRooms()
{
	if (currlevel == 16 || IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
		return;
	}

	if (leveltype != DTYPE_CATHEDRAL) {
		DRLG_HoldThemeRooms();
		return;
	}

	for (int i = 0; i < numthemes; i++) {
		const int8_t v = themes[i].ttval;
		for (int y = 0; y < MAXDUNY; y++) {
			for (int x = 0; x < MAXDUNX; x++) {
				if (dTransVal[x][y] == v) {
					dFlags[x][y] |= DungeonFlag::Populated;
				}
			}
		}
	}
}

void CreateThemeRooms()
{
	if (currlevel == 16 || IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
		return;
	}

	for (int i = 0; i < numthemes; i++) {
		themex = 0;
		themey = 0;
		switch (themes[i].ttype) {
		case THEME_BARREL:
			Theme_Barrel(i);
			break;
		case THEME_SHRINE:
			Theme_Shrine(i);
			break;
		case THEME_MONSTPIT:
			Theme_MonstPit(i);
			break;
		case THEME_SKELROOM:
			Theme_SkelRoom(i);
			break;
		case THEME_TREASURE:
			Theme_Treasure(i);
			break;
		case THEME_LIBRARY:
			Theme_Library(i);
			break;
		case THEME_TORTURE:
			Theme_Torture(i);
			break;
		case THEME_BLOODFOUNTAIN:
			Theme_BloodFountain(i);
			break;
		case THEME_DECAPITATED:
			Theme_Decap(i);
			break;
		case THEME_PURIFYINGFOUNTAIN:
			Theme_PurifyingFountain(i);
			break;
		case THEME_ARMORSTAND:
			Theme_ArmorStand(i);
			break;
		case THEME_GOATSHRINE:
			Theme_GoatShrine(i);
			break;
		case THEME_CAULDRON:
			Theme_Cauldron(i);
			break;
		case THEME_MURKYFOUNTAIN:
			Theme_MurkyFountain(i);
			break;
		case THEME_TEARFOUNTAIN:
			Theme_TearFountain(i);
			break;
		case THEME_BRNCROSS:
			Theme_BrnCross(i);
			break;
		case THEME_WEAPONRACK:
			Theme_WeaponRack(i);
			break;
		case THEME_NONE:
			app_fatal(StrCat("Unknown theme type: ", static_cast<int>(themes[i].ttype)));
		}
	}
	if (leveltype == DTYPE_HELL && themeCount > 0) {
		UpdateL4Trans();
	}
}

} // namespace devilution
