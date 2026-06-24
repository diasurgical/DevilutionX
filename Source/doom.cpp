/**
 * @file doom.cpp
 *
 * Implementation of the map of the stars quest.
 */
#include "doom.h"

#include <optional>

#include "control/control.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/render/renderer.h"

namespace devilution {
namespace {
OptionalOwnedClxSpriteList DoomSprite;
} // namespace

bool DoomFlag;

void doom_init()
{
	DoomSprite = LoadCel("items\\map\\mapztown", 640);
	DoomFlag = true;
}

void doom_close()
{
	DoomFlag = false;
	DoomSprite = std::nullopt;
}

void doom_draw()
{
	if (!DoomFlag) {
		return;
	}

	GetRenderer().DrawClx(GetUIRectangle().position + Displacement { 0, 352 }, (*DoomSprite)[0]);
}

} // namespace devilution
