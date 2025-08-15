/**
 * @file lighting.cpp
 *
 * Implementation of light and vision.
 */
#include "lighting.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <string>

#include <expected.hpp>

#include "automap.h"
#include "engine/displacement.hpp"
#include "engine/lighting_defs.hpp"
#include "engine/load_file.hpp"
#include "engine/point.hpp"
#include "engine/points_in_rectangle_range.hpp"
#include "engine/world_tile.hpp"
#include "levels/tile_properties.hpp"
#include "objects.h"
#include "player.h"
#include "utils/attributes.h"
#include "utils/is_of.hpp"
#include "utils/status_macros.hpp"
#include "vision.hpp"

namespace devilution {

std::array<bool, MAXVISION> VisionActive;
Light VisionList[MAXVISION];
Light Lights[MAXLIGHTS];
std::array<uint8_t, MAXLIGHTS> ActiveLights;
int ActiveLightCount;
std::array<std::array<uint8_t, LightTableSize>, NumLightingLevels> LightTables;
uint8_t *FullyLitLightTable = nullptr;
uint8_t *FullyDarkLightTable = nullptr;
std::array<uint8_t, 256> InfravisionTable;
std::array<uint8_t, 256> StoneTable;
std::array<uint8_t, 256> PauseTable;
#ifdef _DEBUG
bool DisableLighting;
#endif
bool UpdateLighting;

namespace {

/** @brief Number of supported light radiuses (first radius starts with 0) */
constexpr size_t NumLightRadiuses = 16;
/** Falloff tables for the light cone */
uint8_t LightFalloffs[NumLightRadiuses][128];
bool UpdateVision;
/** interpolations of a 32x32 (16x16 mirrored) light circle moving between tiles in steps of 1/8 of a tile */
uint8_t LightConeInterpolations[8][8][16][16];

void RotateRadius(DisplacementOf<int8_t> &offset, DisplacementOf<int8_t> &dist, DisplacementOf<int8_t> &light, DisplacementOf<int8_t> &block)
{
	dist = { static_cast<int8_t>(7 - dist.deltaY), dist.deltaX };
	light = { static_cast<int8_t>(7 - light.deltaY), light.deltaX };
	offset = { static_cast<int8_t>(dist.deltaX - light.deltaX), static_cast<int8_t>(dist.deltaY - light.deltaY) };

	block.deltaX = 0;
	if (offset.deltaX < 0) {
		offset.deltaX += 8;
		block.deltaX = 1;
	}
	block.deltaY = 0;
	if (offset.deltaY < 0) {
		offset.deltaY += 8;
		block.deltaY = 1;
	}
}

DVL_ALWAYS_INLINE void SetLight(Point position, uint8_t v)
{
	if (LoadingMapObjects)
		dPreLight[position.x][position.y] = v;
	else
		dLight[position.x][position.y] = v;
}

DVL_ALWAYS_INLINE uint8_t GetLight(Point position)
{
	if (LoadingMapObjects)
		return dPreLight[position.x][position.y];

	return dLight[position.x][position.y];
}

bool TileAllowsLight(Point position)
{
	if (!InDungeonBounds(position))
		return false;
	return !TileHasAny(position, TileProperties::BlockLight);
}

void DoVisionFlags(Point position, MapExplorationType doAutomap, bool visible)
{
	if (doAutomap != MAP_EXP_NONE) {
		if (dFlags[position.x][position.y] != DungeonFlag::None)
			SetAutomapView(position, doAutomap);
		dFlags[position.x][position.y] |= DungeonFlag::Explored;
	}
	if (visible)
		dFlags[position.x][position.y] |= DungeonFlag::Lit;
	dFlags[position.x][position.y] |= DungeonFlag::Visible;
}

} // namespace

void DoUnLight(Point position, uint8_t radius)
{
	radius++;
	radius++; // If lights moved at a diagonal it can result in some extra tiles being lit

	auto searchArea = PointsInRectangle(WorldTileRectangle { position, radius });

	for (const WorldTilePosition targetPosition : searchArea) {
		if (InDungeonBounds(targetPosition))
			dLight[targetPosition.x][targetPosition.y] = dPreLight[targetPosition.x][targetPosition.y];
	}
}

void DoLighting(Point position, uint8_t radius, DisplacementOf<int8_t> offset)
{
	assert(radius >= 0 && radius <= NumLightRadiuses);
	assert(InDungeonBounds(position));

	DisplacementOf<int8_t> light = {};
	DisplacementOf<int8_t> block = {};

	if (offset.deltaX < 0) {
		offset.deltaX += 8;
		position -= { 1, 0 };
	}
	if (offset.deltaY < 0) {
		offset.deltaY += 8;
		position -= { 0, 1 };
	}

	DisplacementOf<int8_t> dist = offset;

	int minX = 15;
	if (position.x - 15 < 0) {
		minX = position.x + 1;
	}
	int maxX = 15;
	if (position.x + 15 > MAXDUNX) {
		maxX = MAXDUNX - position.x;
	}
	int minY = 15;
	if (position.y - 15 < 0) {
		minY = position.y + 1;
	}
	int maxY = 15;
	if (position.y + 15 > MAXDUNY) {
		maxY = MAXDUNY - position.y;
	}

	// Allow for dim lights in crypt and nest
	if (IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
		if (GetLight(position) > LightFalloffs[radius][0])
			SetLight(position, LightFalloffs[radius][0]);
	} else {
		SetLight(position, 0);
	}

	for (int i = 0; i < 4; i++) {
		const int yBound = i > 0 && i < 3 ? maxY : minY;
		const int xBound = i < 2 ? maxX : minX;
		for (int y = 0; y < yBound; y++) {
			for (int x = 1; x < xBound; x++) {
				const int linearDistance = LightConeInterpolations[offset.deltaX][offset.deltaY][x + block.deltaX][y + block.deltaY];
				if (linearDistance >= 128)
					continue;
				const Point temp = position + (Displacement { x, y }).Rotate(-i);
				const uint8_t v = LightFalloffs[radius][linearDistance];
				if (!InDungeonBounds(temp))
					continue;
				if (v < GetLight(temp))
					SetLight(temp, v);
			}
		}
		RotateRadius(offset, dist, light, block);
	}
}

void DoUnVision(Point position, uint8_t radius)
{
	radius++;
	radius++; // increasing the radius even further here prevents leaving stray vision tiles behind and doesn't seem to affect monster AI - applying new vision happens in the same tick

	auto searchArea = PointsInRectangle(WorldTileRectangle { position, radius });

	for (const WorldTilePosition targetPosition : searchArea) {
		if (InDungeonBounds(targetPosition))
			dFlags[targetPosition.x][targetPosition.y] &= ~(DungeonFlag::Visible | DungeonFlag::Lit);
	}
}

void DoVision(Point position, uint8_t radius, MapExplorationType doAutomap, bool visible)
{
	auto markVisibleFn = [doAutomap, visible](Point rayPoint) {
		DoVisionFlags(rayPoint, doAutomap, visible);
	};
	auto markTransparentFn = [](Point rayPoint) {
		const int8_t trans = dTransVal[rayPoint.x][rayPoint.y];
		if (trans != 0)
			TransList[trans] = true;
	};
	auto passesLightFn = [](Point rayPoint) {
		return TileAllowsLight(rayPoint);
	};
	auto inBoundsFn = [](Point rayPoint) {
		return InDungeonBounds(rayPoint);
	};

	DoVision(position, radius, markVisibleFn, markTransparentFn, passesLightFn, inBoundsFn);
}

tl::expected<void, std::string> LoadTrns()
{
	RETURN_IF_ERROR(LoadFileInMemWithStatus("plrgfx\\infra.trn", InfravisionTable));
	RETURN_IF_ERROR(LoadFileInMemWithStatus("plrgfx\\stone.trn", StoneTable));
	return LoadFileInMemWithStatus("gendata\\pause.trn", PauseTable);
}

void MakeLightTable()
{
	// Generate 16 gradually darker translation tables for doing lighting
	uint8_t shade = 0;
	constexpr uint8_t Black = 0;
	constexpr uint8_t White = 255;
	for (auto &lightTable : LightTables) {
		uint8_t colorIndex = 0;
		for (const uint8_t steps : { 16, 16, 16, 16, 16, 16, 16, 16, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16 }) {
			const uint8_t shading = shade * steps / 16;
			const uint8_t shadeStart = colorIndex;
			const uint8_t shadeEnd = shadeStart + steps - 1;
			for (uint8_t step = 0; step < steps; step++) {
				if (colorIndex == Black) {
					lightTable[colorIndex++] = Black;
					continue;
				}
				int color = shadeStart + step + shading;
				if (color > shadeEnd || color == White)
					color = Black;
				lightTable[colorIndex++] = color;
			}
		}
		shade++;
	}

	LightTables[15] = {}; // Make last shade pitch black
	FullyLitLightTable = LightTables[0].data();
	FullyDarkLightTable = LightTables[LightsMax].data();

	if (leveltype == DTYPE_HELL) {
		// Blood wall lighting
		const auto shades = static_cast<int>(LightTables.size() - 1);
		for (int i = 0; i < shades; i++) {
			auto &lightTable = LightTables[i];
			constexpr int Range = 16;
			for (int j = 0; j < Range; j++) {
				uint8_t color = ((Range - 1) << 4) / shades * (shades - i) / Range * (j + 1);
				color = 1 + (color >> 4);
				int idx = j + 1;
				lightTable[idx] = color;
				idx = 31 - j;
				lightTable[idx] = color;
			}
		}
		FullyLitLightTable = nullptr; // A color map is used for the ceiling animation, so even fully lit tiles have a color map
	} else if (IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
		// Make the lava fully bright
		for (auto &lightTable : LightTables)
			std::iota(lightTable.begin(), lightTable.begin() + 16, uint8_t { 0 });
		LightTables[15][0] = 0;
		std::fill_n(LightTables[15].begin() + 1, 15, 1);
		FullyDarkLightTable = nullptr; // Tiles in Hellfire levels are never completely black
	}

	// Verify that fully lit and fully dark light table optimizations are correctly enabled/disabled (nullptr = disabled)
	assert((FullyLitLightTable != nullptr) == (LightTables[0][0] == 0 && std::adjacent_find(LightTables[0].begin(), LightTables[0].end() - 1, [](auto x, auto y) { return (x + 1) != y; }) == LightTables[0].end() - 1));
	assert((FullyDarkLightTable != nullptr) == (std::all_of(LightTables[LightsMax].begin(), LightTables[LightsMax].end(), [](auto x) { return x == 0; })));

	// Generate light falloffs ranges
	const float maxDarkness = 15;
	const float maxBrightness = 0;
	for (unsigned radius = 0; radius < NumLightRadiuses; radius++) {
		const unsigned maxDistance = (radius + 1) * 8;
		for (unsigned distance = 0; distance < 128; distance++) {
			if (distance > maxDistance) {
				LightFalloffs[radius][distance] = 15;
			} else {
				const float factor = static_cast<float>(distance) / static_cast<float>(maxDistance);
				float scaled;
				if (IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT)) {
					// quardratic falloff with over exposure
					const float brightness = static_cast<float>(radius) * 1.25F;
					scaled = factor * factor * brightness + (maxDarkness - brightness);
					scaled = std::max(maxBrightness, scaled);
				} else {
					// Leaner falloff
					scaled = factor * maxDarkness;
				}
				scaled += 0.5F; // Round up
				LightFalloffs[radius][distance] = static_cast<uint8_t>(scaled);
			}
		}
	}

	// Generate the light cone interpolations
	for (int offsetY = 0; offsetY < 8; offsetY++) {
		for (int offsetX = 0; offsetX < 8; offsetX++) {
			for (int y = 0; y < 16; y++) {
				for (int x = 0; x < 16; x++) {
					const int a = (8 * x - offsetX);
					const int b = (8 * y - offsetY);
					LightConeInterpolations[offsetX][offsetY][x][y] = static_cast<uint8_t>(sqrt(a * a + b * b));
				}
			}
		}
	}
}

#ifdef _DEBUG
void ToggleLighting()
{
	DisableLighting = !DisableLighting;

	if (DisableLighting) {
		memset(dLight, 0, sizeof(dLight));
		return;
	}

	memcpy(dLight, dPreLight, sizeof(dLight));
	for (const Player &player : Players) {
		if (player.plractive && player.isOnActiveLevel()) {
			DoLighting(player.position.tile, player._pLightRad, {});
		}
	}
}
#endif

void InitLighting()
{
	ActiveLightCount = 0;
	UpdateLighting = false;
	UpdateVision = false;
#ifdef _DEBUG
	DisableLighting = false;
#endif

	std::iota(ActiveLights.begin(), ActiveLights.end(), uint8_t { 0 });
	VisionActive = {};
	TransList = {};
}

int AddLight(Point position, uint8_t radius)
{
#ifdef _DEBUG
	if (DisableLighting)
		return NO_LIGHT;
#endif
	if (ActiveLightCount >= MAXLIGHTS)
		return NO_LIGHT;

	const int lid = ActiveLights[ActiveLightCount++];
	Light &light = Lights[lid];
	light.position.tile = position;
	light.radius = radius;
	light.position.offset = { 0, 0 };
	light.isInvalid = false;
	light.hasChanged = false;

	UpdateLighting = true;

	return lid;
}

void AddUnLight(int i)
{
#ifdef _DEBUG
	if (DisableLighting)
		return;
#endif
	if (i == NO_LIGHT)
		return;

	Lights[i].isInvalid = true;

	UpdateLighting = true;
}

void ChangeLightRadius(int i, uint8_t radius)
{
#ifdef _DEBUG
	if (DisableLighting)
		return;
#endif
	if (i == NO_LIGHT)
		return;

	Light &light = Lights[i];
	light.hasChanged = true;
	light.position.old = light.position.tile;
	light.oldRadius = light.radius;
	light.radius = radius;

	UpdateLighting = true;
}

void ChangeLightXY(int i, Point position)
{
#ifdef _DEBUG
	if (DisableLighting)
		return;
#endif
	if (i == NO_LIGHT)
		return;

	Light &light = Lights[i];
	light.hasChanged = true;
	light.position.old = light.position.tile;
	light.oldRadius = light.radius;
	light.position.tile = position;

	UpdateLighting = true;
}

void ChangeLightOffset(int i, DisplacementOf<int8_t> offset)
{
#ifdef _DEBUG
	if (DisableLighting)
		return;
#endif
	if (i == NO_LIGHT)
		return;

	Light &light = Lights[i];
	if (light.position.offset == offset)
		return;

	light.hasChanged = true;
	light.position.old = light.position.tile;
	light.oldRadius = light.radius;
	light.position.offset = offset;

	UpdateLighting = true;
}

void ChangeLight(int i, Point position, uint8_t radius)
{
#ifdef _DEBUG
	if (DisableLighting)
		return;
#endif
	if (i == NO_LIGHT)
		return;

	Light &light = Lights[i];
	light.hasChanged = true;
	light.position.old = light.position.tile;
	light.oldRadius = light.radius;
	light.position.tile = position;
	light.radius = radius;

	UpdateLighting = true;
}

void ProcessLightList()
{
#ifdef _DEBUG
	if (DisableLighting)
		return;
#endif
	if (!UpdateLighting)
		return;
	for (int i = 0; i < ActiveLightCount; i++) {
		Light &light = Lights[ActiveLights[i]];
		if (light.isInvalid) {
			DoUnLight(light.position.tile, light.radius);
		}
		if (light.hasChanged) {
			DoUnLight(light.position.old, light.oldRadius);
			light.hasChanged = false;
		}
	}
	for (int i = 0; i < ActiveLightCount; i++) {
		const Light &light = Lights[ActiveLights[i]];
		if (light.isInvalid) {
			ActiveLightCount--;
			std::swap(ActiveLights[ActiveLightCount], ActiveLights[i]);
			i--;
			continue;
		}
		if (TileHasAny(light.position.tile, TileProperties::Solid))
			continue; // Monster hidden in a wall, don't spoil the surprise
		DoLighting(light.position.tile, light.radius, light.position.offset);
	}

	UpdateLighting = false;
}

void SavePreLighting()
{
	memcpy(dPreLight, dLight, sizeof(dPreLight));
}

void ActivateVision(Point position, int r, size_t id)
{
	auto &vision = VisionList[id];
	vision.position.tile = position;
	vision.radius = r;
	vision.isInvalid = false;
	vision.hasChanged = false;
	VisionActive[id] = true;

	UpdateVision = true;
}

void ChangeVisionRadius(size_t id, int r)
{
	auto &vision = VisionList[id];
	vision.hasChanged = true;
	vision.position.old = vision.position.tile;
	vision.oldRadius = vision.radius;
	vision.radius = r;
	UpdateVision = true;
}

void ChangeVisionXY(size_t id, Point position)
{
	auto &vision = VisionList[id];
	vision.hasChanged = true;
	vision.position.old = vision.position.tile;
	vision.oldRadius = vision.radius;
	vision.position.tile = position;
	UpdateVision = true;
}

void ProcessVisionList()
{
	if (!UpdateVision)
		return;

	TransList = {};

	for (const Player &player : Players) {
		const size_t id = player.getId();
		if (!VisionActive[id])
			continue;
		Light &vision = VisionList[id];
		if (!player.plractive || !player.isOnActiveLevel() || (player._pLvlChanging && &player != MyPlayer)) {
			DoUnVision(vision.position.tile, vision.radius);
			VisionActive[id] = false;
			continue;
		}
		if (vision.hasChanged) {
			DoUnVision(vision.position.old, vision.oldRadius);
			vision.hasChanged = false;
		}
	}
	for (const Player &player : Players) {
		const size_t id = player.getId();
		if (!VisionActive[id])
			continue;
		const Light &vision = VisionList[id];
		MapExplorationType doautomap = MAP_EXP_SELF;
		if (&player != MyPlayer)
			doautomap = player.friendlyMode ? MAP_EXP_OTHERS : MAP_EXP_NONE;
		DoVision(
		    vision.position.tile,
		    vision.radius,
		    doautomap,
		    &player == MyPlayer);
	}

	UpdateVision = false;
}

void lighting_color_cycling()
{
	for (auto &lightTable : LightTables) {
		// shift elements between indexes 1-31 to left
		std::rotate(lightTable.begin() + 1, lightTable.begin() + 2, lightTable.begin() + 32);
	}
}

} // namespace devilution
