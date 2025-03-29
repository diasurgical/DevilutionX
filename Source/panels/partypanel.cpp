#include "panels/partypanel.hpp"

#include <expected.hpp>
#include <optional>

#include "automap.h"
#include "control.h"
#include "engine/backbuffer_state.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/load_clx.hpp"
#include "engine/palette.h"
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/size.hpp"
#include "inv.h"
#include "qol/monhealthbar.h"
#include "qol/stash.h"
#include "pfile.h"
#include "playerdat.hpp"
#include "stores.h"
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
OptionalOwnedClxSpriteList PlayerTags;

Point PartyPanelPos = { 5, 5 };
Rectangle PortraitFrameRects[MAX_PLRS];
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
bool InspectingFromPartyPanel;

tl::expected<void, std::string> LoadPartyPanel()
{
	ASSIGN_OR_RETURN(OwnedClxSpriteList frame, LoadCelWithStatus("data\\textslid", FrameSpriteSize));
	ASSIGN_OR_RETURN(PlayerTags, LoadClxWithStatus("data\\monstertags.clx"));
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
	PlayerTags = std::nullopt;
}

void DrawPartyMemberInfoPanel(const Surface &out)
{
	// Don't draw based on these criteria
	if (CharFlag || !gbIsMultiplayer || !MyPlayer->friendlyMode || IsPlayerInStore() || IsStashOpen)
		return;

	Point pos = PartyPanelPos;
	if (AutomapActive)
		pos.y += PortraitFrameSize.height + FrameGap;

	int currentLongestNameWidth = PortraitFrameSize.width;

	for (Player& player : Players) {

		if (!player.plractive || !player.friendlyMode)
			continue;

#ifndef _DEBUG
		if (&player == MyPlayer)
			continue;
#endif

		// Draw the characters frame
		RenderClxSprite(out, (*PartyMemberFrame)[0], pos);

		// If the player is using mana shield change the value we use to mana
		//	If not use their hitpoints like normal
		int healthOrMana = (player.pManaShield) ? player._pMana : player._pHitPoints;
		int maxHealthOrMana = (player.pManaShield) ? player._pMaxMana : player._pMaxHP;

		// Get the players remaining life
		int lifeTicks = ((healthOrMana * PortraitFrameSize.width) + (maxHealthOrMana / 2)) / maxHealthOrMana;
		uint8_t hpBarColor = (player.pManaShield) ? PAL8_BLUE + 3 : PAL8_RED + 4;
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

		// Draw the player tag
		RenderClxSprite(
			out,
		    (*PlayerTags)[player.getId() + 1],
		    { pos.x + (PortraitFrameSize.width / 2) - 6, pos.y + PortraitFrameSize.height - 8}
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

		// Get the current players name width
		int width = GetLineWidth(player._pName);
		// Now check to see if it's the current longest name
		if (width >= currentLongestNameWidth)
			currentLongestNameWidth = width;

		// Check to see if the Y position is more then the main panel position
		if (pos.y >= GetMainPanel().position.y - PortraitFrameSize.height - 10) {
			// If so we need to draw the next set of portraits back at the top and to the right of the original position
			pos.y = PartyPanelPos.y;
			// Add the current longest name width to the X position
			pos.x += currentLongestNameWidth + (FrameGap / 2);
		}
	}

	if (RightClickedPortraitIndex != -1)
		HandleRightClickPortait();
}

bool DidRightClickPartyPortrait()
{
	for (int i = 0; i < sizeof(PortraitFrameRects) / sizeof(PortraitFrameRects[0]); i++) {
		if (PortraitFrameRects[i].contains(MousePosition)) {
			RightClickedPortraitIndex = i;
			InspectingFromPartyPanel = true;
			return true;
		}
	}

	return false;
}

} // namespace devilution
