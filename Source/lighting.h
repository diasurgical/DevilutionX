/**
 * @file lighting.h
 *
 * Interface of light and vision.
 */
#pragma once

#include <array>
#include <cstdint>

#include <expected.hpp>

#include "automap.h"
#include "engine/displacement.hpp"
#include "engine/point.hpp"
#include "engine/world_tile.hpp"
#include "utils/attributes.h"

namespace devilution {

#define MAXLIGHTS 32
#define MAXVISION 4
/** @brief Number of supported light levels */
constexpr size_t NumLightingLevels = 16;
#define NO_LIGHT -1

struct LightPosition {
	WorldTilePosition tile;
	/** Pixel offset from tile. */
	DisplacementOf<int8_t> offset;
	/** Previous position. */
	WorldTilePosition old;
};

struct Light {
	LightPosition position;
	uint8_t radius;
	uint8_t oldRadius;
	bool isInvalid;
	bool hasChanged;
};

extern Light VisionList[MAXVISION];
extern std::array<bool, MAXVISION> VisionActive;
extern Light Lights[MAXLIGHTS];
extern std::array<uint8_t, MAXLIGHTS> ActiveLights;
extern int ActiveLightCount;
constexpr char LightsMax = 15;
extern DVL_API_FOR_TEST std::array<std::array<uint8_t, 256>, NumLightingLevels> LightTables;
/** @brief Contains a pointer to a light table that is fully lit (no color mapping is required). Can be null in hell. */
extern DVL_API_FOR_TEST uint8_t *FullyLitLightTable;
/** @brief Contains a pointer to a light table that is fully dark (every color result to 0/black). Can be null in hellfire levels. */
extern DVL_API_FOR_TEST uint8_t *FullyDarkLightTable;
extern std::array<uint8_t, 256> InfravisionTable;
extern std::array<uint8_t, 256> StoneTable;
extern std::array<uint8_t, 256> PauseTable;
#ifdef _DEBUG
extern bool DisableLighting;
#endif
extern bool UpdateLighting;

void DoUnLight(Point position, uint8_t radius);
void DoLighting(Point position, uint8_t radius, DisplacementOf<int8_t> offset);
void DoUnVision(Point position, uint8_t radius);
void DoVision(Point position, uint8_t radius, MapExplorationType doAutomap, bool visible);
tl::expected<void, std::string> LoadTrns();
void MakeLightTable();
#ifdef _DEBUG
void ToggleLighting();
#endif
void InitLighting();
int AddLight(Point position, uint8_t radius);
void AddUnLight(int i);
void ChangeLightRadius(int i, uint8_t radius);
void ChangeLightXY(int i, Point position);
void ChangeLightOffset(int i, DisplacementOf<int8_t> offset);
void ChangeLight(int i, Point position, uint8_t radius);
void ProcessLightList();
void SavePreLighting();
void ActivateVision(Point position, int r, size_t id);
void ChangeVisionRadius(size_t id, int r);
void ChangeVisionXY(size_t id, Point position);
void ProcessVisionList();
void lighting_color_cycling();

constexpr int MaxCrawlRadius = 18;

} // namespace devilution
