#include "panels/partypanel.hpp"

#include <expected.hpp>
#include <optional>

#include "control.h"
#include "engine/backbuffer_state.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/palette.h"
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/size.hpp"
#include "inv.h"
#include "pfile.h"
#include "playerdat.hpp"
#include <SDL_rect.h>
#include "utils/status_macros.hpp"
#include "utils/surface_to_clx.hpp"

namespace devilution {

namespace {

struct PartySpriteOffset {
	Point inTownOffset;
	Point inDungeonOffset;
	Point isDeadOffset;
};

const PartySpriteOffset ClassSpriteOffsets[] = {
	{ { -4, -18 }, { 6, -21 }, { -6, -50 } },
	{ { -2, -18 }, { 1, -20 }, { -8, -35 } },
	{ { -2, -16 }, { 3, -20 }, { 0, -50 } },
	{ { -2, -19 }, { 1, -19 }, { 28, -60 } }
};

OptionalOwnedClxSpriteList PartyMemberFrame;
Point PartyPanelPos = { 5, 5 };
Rectangle PortraitFrameRects[9];
int RightClickedPortraitIndex = -1;
constexpr int HealthBarHeight = 7;
constexpr int FrameGap = 25;
constexpr int FrameBorderSize = 3;
constexpr int FrameSpriteSize = 12;
constexpr Size FrameSections = { 4, 4 }; // x/y can't be less than 2
constexpr Size PortraitFrameSize = { FrameSections.width * FrameSpriteSize, FrameSections.height * FrameSpriteSize };

constexpr uint8_t FrameBackgroundColor = PAL16_BLUE + 14;

void DrawBar(const Surface& out, Rectangle rect, uint8_t color)
{
	for (int x = 0; x < rect.size.width; x++) {
		DrawVerticalLine(out, { rect.position.x + x, rect.position.y }, rect.size.height, color);
	}
}

void DrawMemberFrame(const Surface &out, OwnedClxSpriteList &frame, Point pos)
{
	// Draw the frame background
	FillRect(out, pos.x, pos.y, PortraitFrameSize.width, PortraitFrameSize.height, FrameBackgroundColor);

	// Now draw the frame border
	Size adjustedFrame = { FrameSections.width - 1, FrameSections.height - 1 };
	for (int x = 0; x <= adjustedFrame.width; x++) {
		for (int y = 0; y <= adjustedFrame.height; y++) {
			// Get what section of the frame we're drawing
			int spriteIndex = -1;
			if (x == 0 && y == 0)
				spriteIndex = 0; // Top-left corner
			else if (x == 0 && y == adjustedFrame.height)
				spriteIndex = 1; // Bottom-left corner
			else if (x == adjustedFrame.width && y == adjustedFrame.height)
				spriteIndex = 2; // Bottom-right corner
			else if (x == adjustedFrame.width && y == 0)
				spriteIndex = 3; // Top-right corner
			else if (y == 0)
				spriteIndex = 4; // Top border
			else if (x == 0)
				spriteIndex = 5; // Left border
			else if (y == adjustedFrame.height)
				spriteIndex = 6; // Bottom border
			else if (x == adjustedFrame.width)
				spriteIndex = 7; // Right border

			if (spriteIndex != -1) {
				// Draw the frame section
				RenderClxSprite(out, frame[spriteIndex], { pos.x + (x * FrameSpriteSize), pos.y + (y * FrameSpriteSize) });
			}
		}
	}
}

void HandleRightClickPortait()
{
	Player &player = Players[RightClickedPortraitIndex];
	if (player.plractive && &player != MyPlayer) {
		InspectPlayer = &player;
		OpenCharPanel();
		if (!SpellbookFlag)
			invflag = true;
		RedrawEverything();
		RightClickedPortraitIndex = -1;
	}
}

PartySpriteOffset GetClassSpriteOffset(HeroClass hClass)
{
	switch (hClass) {
	case HeroClass::Bard:
		hClass = HeroClass::Rogue;
		break;
	case HeroClass::Barbarian:
		hClass = HeroClass::Warrior;
		break;
	}

	return ClassSpriteOffsets[static_cast<size_t>(hClass)];
}

} // namespace

bool PartySidePanelOpen = true;

tl::expected<void, std::string> LoadPartyPanel()
{
	ASSIGN_OR_RETURN(OwnedClxSpriteList frame, LoadCelWithStatus("data\\textslid", FrameSpriteSize));
	OwnedSurface out(PortraitFrameSize.width, PortraitFrameSize.height + HealthBarHeight);

	// Draw the health bar background
	DrawBar(out, { { 0, 0 }, { PortraitFrameSize.width, HealthBarHeight } }, PAL16_GRAY + 10);
	// Draw the frame the character portrait sprite will go
	DrawMemberFrame(out, frame, { 0, HealthBarHeight });

	PartyMemberFrame = SurfaceToClx(out);
	return {};
}

void FreePartyPanel()
{
	PartyMemberFrame = std::nullopt;
}

void DrawPartyMemberInfoPanel(const Surface &out)
{
	// Don't draw if certain panels are open
	if (CharFlag)
		return;

	if (!gbIsMultiplayer)
		return;

	Point pos = PartyPanelPos;

	for (Player& player : Players) {

		if (!player.plractive)
			continue;

#ifndef _DEBUG
		if (&player == MyPlayer)
			continue;
#endif

		// Draw the characters frame
		RenderClxSprite(out, (*PartyMemberFrame)[0], pos);

		// Get the players remaining life
		int lifeTicks = ((player._pHitPoints * PortraitFrameSize.width) + (player._pMaxHP / 2)) / player._pMaxHP;
		uint8_t hpBarColor = PAL8_RED + 4;
		// Now draw the characters remaining life
		DrawBar(out, { pos, { lifeTicks, HealthBarHeight } }, hpBarColor);

		// Add to the position before continuing to the next item
		pos.y += HealthBarHeight;

		// Get the players current portrait sprite
		ClxSprite playerPortraitSprite = GetPlayerPortraitSprite(player);
		// Get the offset of the sprite based on the players class so it get's rendered in the correct position
		PartySpriteOffset offsets = GetClassSpriteOffset(player._pClass);
		Point offset = (player.isOnLevel(0)) ? offsets.inTownOffset : offsets.inDungeonOffset;

		if (player._pHitPoints <= 0 && IsPlayerUnarmed(player))
			offset = offsets.isDeadOffset;

		// Calculate the players portait position
		Point portraitPos = { ((-(playerPortraitSprite.width() / 2)) + (PortraitFrameSize.width / 2)) + offset.x, offset.y };
		// Get a subregion of the surface so the portrait doesn't get drawn over the frame
		Surface frameSubregion = out.subregion(
			pos.x + FrameBorderSize,
		    pos.y + FrameBorderSize,
		    PortraitFrameSize.width - (FrameBorderSize * 2),
		    PortraitFrameSize.height - (FrameBorderSize * 2)
		);

		PortraitFrameRects[player.getId()] = {
			{ frameSubregion.region.x, frameSubregion.region.y },
			{ frameSubregion.region.w, frameSubregion.region.h }
		};

		// Draw the portrait sprite
		RenderClxSprite(
			frameSubregion,
		    playerPortraitSprite,
			portraitPos
		);

		// Check to see if the player is dead and if so we draw a half transparent red rect over the portrait
		if (player._pHitPoints <= 0) {
			DrawHalfTransparentRectTo(
			    frameSubregion,
			    0, 0,
			    PortraitFrameSize.width,
			    PortraitFrameSize.height,
			    PAL8_RED + 4
			);
		}

		//HandleInputActions(player, { { frameSubregion.region.x, frameSubregion.region.y }, { frameSubregion.region.w, frameSubregion.region.h } });

		// Add to the position before continuing to the next item
		pos.y += PortraitFrameSize.height + 4;

		// Draw the players name under the frame
		DrawString(
			out,
			player._pName,
			pos,
			{ .flags = UiFlags::ColorGold | UiFlags::Outlined | UiFlags::FontSize12 }
		);

		// Add to the position before continuing onto the next player
		pos.y += FrameGap;
	}

	if (RightClickedPortraitIndex != -1)
		HandleRightClickPortait();
}

bool DidRightClickPartyPortrait()
{
	for (int i = 0; i < sizeof(PortraitFrameRects); i++) {
		if (PortraitFrameRects[i].contains(MousePosition)) {
			RightClickedPortraitIndex = i;
			return true;
		}
	}

	return false;
}

} // namespace devilution
