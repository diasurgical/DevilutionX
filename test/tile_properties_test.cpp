#include "levels/tile_properties.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "levels/dun_tile.hpp"
#include "levels/gendung.h"
#include "objects.h"
#include "tables/objdat.h"

namespace devilution {
namespace {

TEST(TilePropertiesTest, Solid)
{
	dPiece[5][5] = 0;
	SOLData[0] = TileProperties::Solid;
	EXPECT_TRUE(IsTileSolid({ 5, 5 })) << "Solid in-bounds tiles are solid";
	EXPECT_FALSE(IsTileNotSolid({ 5, 5 })) << "IsTileNotSolid returns the inverse of IsTileSolid for in-bounds tiles";

	dPiece[6][6] = 1;
	SOLData[1] = TileProperties::None;
	EXPECT_FALSE(IsTileSolid({ 6, 6 })) << "Non-solid in-bounds tiles are not solid";
	EXPECT_TRUE(IsTileNotSolid({ 6, 6 })) << "IsTileNotSolid returns the inverse of IsTileSolid for in-bounds tiles";

	EXPECT_FALSE(IsTileSolid({ -1, 1 })) << "Out of bounds tiles are not solid"; // this reads out of bounds in the current code and may fail unexpectedly
	EXPECT_FALSE(IsTileNotSolid({ -1, 1 })) << "Out of bounds tiles are also not not solid";
}

TEST(TilePropertiesTest, Walkable)
{
	dPiece[5][5] = 0;
	SOLData[0] = TileProperties::Solid; // Doing this manually to save running through the code in gendung.cpp
	EXPECT_FALSE(IsTileWalkable({ 5, 5 })) << "Tile which is marked as solid should be considered blocked";
	EXPECT_FALSE(IsTileWalkable({ 5, 5 }, true)) << "Solid non-door tiles remain unwalkable when ignoring doors";

	SOLData[0] = TileProperties::None;
	EXPECT_TRUE(IsTileWalkable({ 5, 5 })) << "Non-solid tiles are walkable";
	EXPECT_TRUE(IsTileWalkable({ 5, 5 }, true)) << "Non-solid tiles remain walkable when ignoring doors";

	dObject[5][5] = 1;
	Objects[0]._oSolidFlag = true;
	EXPECT_FALSE(IsTileWalkable({ 5, 5 })) << "Tile occupied by a solid object is unwalkable";
	EXPECT_FALSE(IsTileWalkable({ 5, 5 }, true)) << "Tile occupied by a solid non-door object are unwalkable when ignoring doors";

	Objects[0]._otype = _object_id::OBJ_L1LDOOR;
	EXPECT_FALSE(IsTileWalkable({ 5, 5 })) << "Tile occupied by a door which is marked as solid should be considered blocked";
	EXPECT_TRUE(IsTileWalkable({ 5, 5 }, true)) << "Tile occupied by a door is considered walkable when ignoring doors";

	Objects[0]._oSolidFlag = false;
	EXPECT_TRUE(IsTileWalkable({ 5, 5 })) << "Tile occupied by an open door is walkable";
	EXPECT_TRUE(IsTileWalkable({ 5, 5 }, true)) << "Tile occupied by a door is considered walkable when ignoring doors";

	SOLData[0] = TileProperties::Solid;
	EXPECT_FALSE(IsTileWalkable({ 5, 5 })) << "Solid tiles occupied by an open door remain unwalkable";
	EXPECT_TRUE(IsTileWalkable({ 5, 5 }, true)) << "Solid tiles occupied by an open door become walkable when ignoring doors";
}

TEST(TilePropertiesTest, DoorArchwaySolidTileBecomesWalkableWhenIgnoringDoors)
{
	dPiece[5][4] = 0;
	SOLData[0] = TileProperties::Solid;
	dObject[5][4] = 0;
	EXPECT_FALSE(IsTileWalkable({ 5, 4 }, true))
	    << "Solid tile with no object is unwalkable even when ignoring doors";

	Objects[0]._otype = _object_id::OBJ_L1LDOOR;
	Objects[0]._oSolidFlag = false;
	dObject[5][5] = 1;
	dObject[5][4] = -1; // Negative dObject value: extended area of Objects[0] (the door)
	EXPECT_TRUE(IsTileWalkable({ 5, 4 }, true))
	    << "Solid archway tile referencing a door becomes walkable when ignoring doors";
	EXPECT_FALSE(IsTileWalkable({ 5, 4 }))
	    << "Solid archway tile referencing a door is still unwalkable normally";

	// Cleanup
	dObject[5][5] = 0;
	dObject[5][4] = 0;
	Objects[0] = {};
}

TEST(TilePropertiesTest, CanStepTest)
{
	dPiece[0][0] = 0;
	dPiece[0][1] = 0;
	dPiece[1][0] = 0;
	dPiece[1][1] = 0;
	SOLData[0] = TileProperties::None;
	EXPECT_TRUE(CanStep({ 0, 0 }, { 1, 1 })) << "A step in open space is free of solid pieces";
	EXPECT_TRUE(CanStep({ 1, 1 }, { 0, 0 })) << "A step in open space is free of solid pieces";
	EXPECT_TRUE(CanStep({ 1, 0 }, { 0, 1 })) << "A step in open space is free of solid pieces";
	EXPECT_TRUE(CanStep({ 0, 1 }, { 1, 0 })) << "A step in open space is free of solid pieces";

	SOLData[1] = TileProperties::Solid;
	dPiece[1][0] = 1;
	EXPECT_TRUE(CanStep({ 0, 1 }, { 1, 0 })) << "Can path to a destination which is solid";
	EXPECT_TRUE(CanStep({ 1, 0 }, { 0, 1 })) << "Can path from a starting position which is solid";
	EXPECT_TRUE(CanStep({ 0, 1 }, { 1, 1 })) << "Stepping in a cardinal direction ignores solid pieces";
	EXPECT_TRUE(CanStep({ 1, 0 }, { 1, 1 })) << "Stepping in a cardinal direction ignores solid pieces";
	EXPECT_TRUE(CanStep({ 0, 0 }, { 1, 0 })) << "Stepping in a cardinal direction ignores solid pieces";
	EXPECT_TRUE(CanStep({ 1, 1 }, { 1, 0 })) << "Stepping in a cardinal direction ignores solid pieces";

	EXPECT_FALSE(CanStep({ 0, 0 }, { 1, 1 })) << "Can't cut a solid corner";
	EXPECT_FALSE(CanStep({ 1, 1 }, { 0, 0 })) << "Can't cut a solid corner";
	dPiece[0][1] = 1;
	EXPECT_FALSE(CanStep({ 0, 0 }, { 1, 1 })) << "Can't walk through the boundary between two corners";
	EXPECT_FALSE(CanStep({ 1, 1 }, { 0, 0 })) << "Can't walk through the boundary between two corners";
	dPiece[1][0] = 0;
	EXPECT_FALSE(CanStep({ 0, 0 }, { 1, 1 })) << "Can't cut a solid corner";
	EXPECT_FALSE(CanStep({ 1, 1 }, { 0, 0 })) << "Can't cut a solid corner";
	dPiece[0][1] = 0;

	dPiece[0][0] = 1;
	EXPECT_FALSE(CanStep({ 1, 0 }, { 0, 1 })) << "Can't cut a solid corner";
	EXPECT_FALSE(CanStep({ 0, 1 }, { 1, 0 })) << "Can't cut a solid corner";
	dPiece[1][1] = 1;
	EXPECT_FALSE(CanStep({ 1, 0 }, { 0, 1 })) << "Can't walk through the boundary between two corners";
	EXPECT_FALSE(CanStep({ 0, 1 }, { 1, 0 })) << "Can't walk through the boundary between two corners";
	dPiece[0][0] = 0;
	EXPECT_FALSE(CanStep({ 1, 0 }, { 0, 1 })) << "Can't cut a solid corner";
	EXPECT_FALSE(CanStep({ 0, 1 }, { 1, 0 })) << "Can't cut a solid corner";
	dPiece[1][1] = 0;
}

} // namespace
} // namespace devilution
