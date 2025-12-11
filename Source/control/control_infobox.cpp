/**
 * @file control_flasks.cpp
 *
 * Implementation of the info box
 */
#include "control.hpp"

#include "engine/render/primitive_render.hpp"
#include "inv.h"
#include "levels/trigs.h"
#include "panels/partypanel.hpp"
#include "qol/stash.h"
#include "qol/xpbar.h"
#include "towners.h"
#include "utils/algorithm/container.hpp"
#include "utils/format_int.hpp"
#include "utils/log.hpp"
#include "utils/screen_reader.hpp"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"

namespace devilution {

StringOrView InfoString;
StringOrView FloatingInfoString;

namespace {

void PrintInfo(const Surface &out)
{
	if (ChatFlag)
		return;

	const int space[] = { 18, 12, 6, 3, 0 };
	Rectangle infoBox = InfoBoxRect;

	SetPanelObjectPosition(UiPanels::Main, infoBox);

	const auto newLineCount = static_cast<int>(c_count(InfoString.str(), '\n'));
	const int spaceIndex = std::min(4, newLineCount);
	const int spacing = space[spaceIndex];
	const int lineHeight = 12 + spacing;

	// Adjusting the line height to add spacing between lines
	// will also add additional space beneath the last line
	// which throws off the vertical centering
	infoBox.position.y += spacing / 2;

	SpeakText(InfoString);

	DrawString(out, InfoString, infoBox,
	    {
	        .flags = InfoColor | UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::KerningFitSpacing,
	        .spacing = 2,
	        .lineHeight = lineHeight,
	    });
}

Rectangle GetFloatingInfoRect(const int lineHeight, const int textSpacing)
{
	// Calculate the width and height of the floating info box
	const std::string txt = std::string(FloatingInfoString);

	auto lines = SplitByChar(txt, '\n');
	const GameFontTables font = GameFont12;
	int maxW = 0;

	for (const auto &line : lines) {
		const int w = GetLineWidth(line, font, textSpacing, nullptr);
		maxW = std::max(maxW, w);
	}

	const auto lineCount = 1 + static_cast<int>(c_count(FloatingInfoString.str(), '\n'));
	const int totalH = lineCount * lineHeight;

	const Player &player = *InspectPlayer;

	// 1) Equipment (Rect position)
	if (pcursinvitem >= INVITEM_HEAD && pcursinvitem < INVITEM_INV_FIRST) {
		const int slot = pcursinvitem - INVITEM_HEAD;
		static constexpr Point equipLocal[] = {
			{ 133, 59 },
			{ 48, 205 },
			{ 249, 205 },
			{ 205, 60 },
			{ 17, 160 },
			{ 248, 160 },
			{ 133, 160 },
		};

		Point itemPosition = equipLocal[slot];
		auto &item = player.InvBody[slot];
		const Size frame = GetInvItemSize(item._iCurs + CURSOR_FIRSTITEM);

		if (slot == INVLOC_HAND_LEFT) {
			itemPosition.x += frame.width == InventorySlotSizeInPixels.width
			    ? InventorySlotSizeInPixels.width
			    : 0;
			itemPosition.y += frame.height == 3 * InventorySlotSizeInPixels.height
			    ? 0
			    : -InventorySlotSizeInPixels.height;
		} else if (slot == INVLOC_HAND_RIGHT) {
			itemPosition.x += frame.width == InventorySlotSizeInPixels.width
			    ? (InventorySlotSizeInPixels.width - 1)
			    : 1;
			itemPosition.y += frame.height == 3 * InventorySlotSizeInPixels.height
			    ? 0
			    : -InventorySlotSizeInPixels.height;
		}

		itemPosition.y++;                  // Align position to bottom left of the item graphic
		itemPosition.x += frame.width / 2; // Align position to center of the item graphic
		itemPosition.x -= maxW / 2;        // Align position to the center of the floating item info box

		const Point screen = GetPanelPosition(UiPanels::Inventory, itemPosition);

		return { { screen.x, screen.y }, { maxW, totalH } };
	}

	// 2) Inventory grid (Rect position)
	if (pcursinvitem >= INVITEM_INV_FIRST && pcursinvitem < INVITEM_INV_FIRST + InventoryGridCells) {
		const int itemIdx = pcursinvitem - INVITEM_INV_FIRST;

		for (int j = 0; j < InventoryGridCells; ++j) {
			if (player.InvGrid[j] > 0 && player.InvGrid[j] - 1 == itemIdx) {
				const Item &it = player.InvList[itemIdx];
				Point itemPosition = InvRect[j + SLOTXY_INV_FIRST].position;

				itemPosition.x += GetInventorySize(it).width * InventorySlotSizeInPixels.width / 2; // Align position to center of the item graphic
				itemPosition.x -= maxW / 2;                                                         // Align position to the center of the floating item info box

				const Point screen = GetPanelPosition(UiPanels::Inventory, itemPosition);

				return { { screen.x, screen.y }, { maxW, totalH } };
			}
		}
	}

	// 3) Belt (Rect position)
	if (pcursinvitem >= INVITEM_BELT_FIRST && pcursinvitem < INVITEM_BELT_FIRST + MaxBeltItems) {
		const int itemIdx = pcursinvitem - INVITEM_BELT_FIRST;
		for (int i = 0; i < MaxBeltItems; ++i) {
			if (player.SpdList[i].isEmpty())
				continue;
			if (i != itemIdx)
				continue;

			const Item &item = player.SpdList[i];
			Point itemPosition = InvRect[i + SLOTXY_BELT_FIRST].position;

			itemPosition.x += GetInventorySize(item).width * InventorySlotSizeInPixels.width / 2; // Align position to center of the item graphic
			itemPosition.x -= maxW / 2;                                                           // Align position to the center of the floating item info box

			const Point screen = GetMainPanel().position + Displacement { itemPosition.x, itemPosition.y };

			return { { screen.x, screen.y }, { maxW, totalH } };
		}
	}

	// 4) Stash (Rect position)
	if (pcursstashitem != StashStruct::EmptyCell) {
		for (auto slot : StashGridRange) {
			auto itemId = Stash.GetItemIdAtPosition(slot);
			if (itemId == StashStruct::EmptyCell)
				continue;
			if (itemId != pcursstashitem)
				continue;

			const Item &item = Stash.stashList[itemId];
			Point itemPosition = GetStashSlotCoord(slot);
			const Size itemGridSize = GetInventorySize(item);

			itemPosition.y += itemGridSize.height * (InventorySlotSizeInPixels.height + 1) - 1; // Align position to bottom left of the item graphic
			itemPosition.x += itemGridSize.width * InventorySlotSizeInPixels.width / 2;         // Align position to center of the item graphic
			itemPosition.x -= maxW / 2;                                                         // Align position to the center of the floating item info box

			return { { itemPosition.x, itemPosition.y }, { maxW, totalH } };
		}
	}

	return { { 0, 0 }, { 0, 0 } };
}

int GetHoverSpriteHeight()
{
	if (pcursinvitem >= INVITEM_HEAD && pcursinvitem < INVITEM_INV_FIRST) {
		auto &it = (*InspectPlayer).InvBody[pcursinvitem - INVITEM_HEAD];
		return GetInvItemSize(it._iCurs + CURSOR_FIRSTITEM).height + 1;
	}
	if (pcursinvitem >= INVITEM_INV_FIRST
	    && pcursinvitem < INVITEM_INV_FIRST + InventoryGridCells) {
		const int idx = pcursinvitem - INVITEM_INV_FIRST;
		auto &it = (*InspectPlayer).InvList[idx];
		return GetInventorySize(it).height * (InventorySlotSizeInPixels.height + 1)
		    - InventorySlotSizeInPixels.height;
	}
	if (pcursinvitem >= INVITEM_BELT_FIRST
	    && pcursinvitem < INVITEM_BELT_FIRST + MaxBeltItems) {
		const int idx = pcursinvitem - INVITEM_BELT_FIRST;
		auto &it = (*InspectPlayer).SpdList[idx];
		return GetInventorySize(it).height * (InventorySlotSizeInPixels.height + 1)
		    - InventorySlotSizeInPixels.height - 1;
	}
	if (pcursstashitem != StashStruct::EmptyCell) {
		auto &it = Stash.stashList[pcursstashitem];
		return GetInventorySize(it).height * (InventorySlotSizeInPixels.height + 1);
	}
	return InventorySlotSizeInPixels.height;
}

int ClampAboveOrBelow(int anchorY, int spriteH, int boxH, int pad, int linePad)
{
	const int yAbove = anchorY - spriteH - boxH - pad;
	const int yBelow = anchorY + linePad / 2 + pad;
	return (yAbove >= 0) ? yAbove : yBelow;
}

void PrintFloatingInfo(const Surface &out)
{
	if (ChatFlag)
		return;
	if (FloatingInfoString.empty())
		return;

	const int verticalSpacing = 3;
	const int lineHeight = 12 + verticalSpacing;
	const int textSpacing = 2;
	const int hPadding = 5;
	const int vPadding = 4;

	Rectangle floatingInfoBox = GetFloatingInfoRect(lineHeight, textSpacing);

	// Prevent the floating info box from going off-screen horizontally
	floatingInfoBox.position.x = std::clamp(floatingInfoBox.position.x, hPadding, GetScreenWidth() - (floatingInfoBox.size.width + hPadding));

	const int spriteH = GetHoverSpriteHeight();
	const int anchorY = floatingInfoBox.position.y;

	// Prevent the floating info box from going off-screen vertically
	floatingInfoBox.position.y = ClampAboveOrBelow(anchorY, spriteH, floatingInfoBox.size.height, vPadding, verticalSpacing);

	SpeakText(FloatingInfoString);

	for (int i = 0; i < 3; i++)
		DrawHalfTransparentRectTo(out, floatingInfoBox.position.x - hPadding, floatingInfoBox.position.y - vPadding, floatingInfoBox.size.width + hPadding * 2, floatingInfoBox.size.height + vPadding * 2);
	DrawHalfTransparentVerticalLine(out, { floatingInfoBox.position.x - hPadding - 1, floatingInfoBox.position.y - vPadding - 1 }, floatingInfoBox.size.height + (vPadding * 2) + 2, PAL16_GRAY + 10);
	DrawHalfTransparentVerticalLine(out, { floatingInfoBox.position.x + hPadding + floatingInfoBox.size.width, floatingInfoBox.position.y - vPadding - 1 }, floatingInfoBox.size.height + (vPadding * 2) + 2, PAL16_GRAY + 10);
	DrawHalfTransparentHorizontalLine(out, { floatingInfoBox.position.x - hPadding, floatingInfoBox.position.y - vPadding - 1 }, floatingInfoBox.size.width + (hPadding * 2), PAL16_GRAY + 10);
	DrawHalfTransparentHorizontalLine(out, { floatingInfoBox.position.x - hPadding, floatingInfoBox.position.y + vPadding + floatingInfoBox.size.height }, floatingInfoBox.size.width + (hPadding * 2), PAL16_GRAY + 10);

	DrawString(out, FloatingInfoString, floatingInfoBox,
	    {
	        .flags = InfoColor | UiFlags::AlignCenter | UiFlags::VerticalCenter,
	        .spacing = textSpacing,
	        .lineHeight = lineHeight,
	    });
}

} // namespace

void AddInfoBoxString(std::string_view str, bool floatingBox /*= false*/)
{
	StringOrView &infoString = floatingBox ? FloatingInfoString : InfoString;

	if (infoString.empty())
		infoString = str;
	else
		infoString = StrCat(infoString, "\n", str);
}

void AddInfoBoxString(std::string &&str, bool floatingBox /*= false*/)
{
	StringOrView &infoString = floatingBox ? FloatingInfoString : InfoString;

	if (infoString.empty())
		infoString = std::move(str);
	else
		infoString = StrCat(infoString, "\n", str);
}

void CheckPanelInfo()
{
	MainPanelFlag = false;
	InfoString = StringOrView {};
	FloatingInfoString = StringOrView {};

	const int totalButtons = IsChatAvailable() ? TotalMpMainPanelButtons : TotalSpMainPanelButtons;

	for (int i = 0; i < totalButtons; i++) {
		Rectangle button = MainPanelButtonRect[i];

		SetPanelObjectPosition(UiPanels::Main, button);

		if (button.contains(MousePosition)) {
			if (i != 7) {
				InfoString = _(PanBtnStr[i]);
			} else {
				if (MyPlayer->friendlyMode)
					InfoString = _("Player friendly");
				else
					InfoString = _("Player attack");
			}
			if (PanBtnHotKey[i] != nullptr) {
				AddInfoBoxString(fmt::format(fmt::runtime(_("Hotkey: {:s}")), _(PanBtnHotKey[i])));
			}
			InfoColor = UiFlags::ColorWhite;
			MainPanelFlag = true;
		}
	}

	Rectangle spellSelectButton = SpellButtonRect;

	SetPanelObjectPosition(UiPanels::Main, spellSelectButton);

	if (!SpellSelectFlag && spellSelectButton.contains(MousePosition)) {
		InfoString = _("Select current spell button");
		InfoColor = UiFlags::ColorWhite;
		MainPanelFlag = true;
		AddInfoBoxString(_("Hotkey: 's'"));
		const Player &myPlayer = *MyPlayer;
		const SpellID spellId = myPlayer._pRSpell;
		if (IsValidSpell(spellId)) {
			switch (myPlayer._pRSplType) {
			case SpellType::Skill:
				AddInfoBoxString(fmt::format(fmt::runtime(_("{:s} Skill")), pgettext("spell", GetSpellData(spellId).sNameText)));
				break;
			case SpellType::Spell: {
				AddInfoBoxString(fmt::format(fmt::runtime(_("{:s} Spell")), pgettext("spell", GetSpellData(spellId).sNameText)));
				const int spellLevel = myPlayer.GetSpellLevel(spellId);
				AddInfoBoxString(spellLevel == 0 ? _("Spell Level 0 - Unusable") : fmt::format(fmt::runtime(_("Spell Level {:d}")), spellLevel));
			} break;
			case SpellType::Scroll: {
				AddInfoBoxString(fmt::format(fmt::runtime(_("Scroll of {:s}")), pgettext("spell", GetSpellData(spellId).sNameText)));
				const int scrollCount = c_count_if(InventoryAndBeltPlayerItemsRange { myPlayer }, [spellId](const Item &item) {
					return item.isScrollOf(spellId);
				});
				AddInfoBoxString(fmt::format(fmt::runtime(ngettext("{:d} Scroll", "{:d} Scrolls", scrollCount)), scrollCount));
			} break;
			case SpellType::Charges:
				AddInfoBoxString(fmt::format(fmt::runtime(_("Staff of {:s}")), pgettext("spell", GetSpellData(spellId).sNameText)));
				AddInfoBoxString(fmt::format(fmt::runtime(ngettext("{:d} Charge", "{:d} Charges", myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges)), myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges));
				break;
			case SpellType::Invalid:
				break;
			}
		}
	}

	Rectangle belt = BeltRect;

	SetPanelObjectPosition(UiPanels::Main, belt);

	if (belt.contains(MousePosition))
		pcursinvitem = CheckInvHLight();

	if (CheckXPBarInfo())
		MainPanelFlag = true;
}

void DrawInfoBox(const Surface &out)
{
	DrawPanelBox(out, MakeSdlRect(InfoBoxRect.position.x, InfoBoxRect.position.y + PanelPaddingHeight, InfoBoxRect.size.width, InfoBoxRect.size.height), GetMainPanel().position + Displacement { InfoBoxRect.position.x, InfoBoxRect.position.y });
	if (!MainPanelFlag && !trigflag && pcursinvitem == -1 && pcursstashitem == StashStruct::EmptyCell && !SpellSelectFlag && pcurs != CURSOR_HOURGLASS) {
		InfoString = StringOrView {};
		InfoColor = UiFlags::ColorWhite;
	}
	const Player &myPlayer = *MyPlayer;
	if (SpellSelectFlag || trigflag || pcurs == CURSOR_HOURGLASS) {
		InfoColor = UiFlags::ColorWhite;
	} else if (!myPlayer.HoldItem.isEmpty()) {
		if (myPlayer.HoldItem._itype == ItemType::Gold) {
			const int nGold = myPlayer.HoldItem._ivalue;
			InfoString = fmt::format(fmt::runtime(ngettext("{:s} gold piece", "{:s} gold pieces", nGold)), FormatInteger(nGold));
		} else if (!myPlayer.CanUseItem(myPlayer.HoldItem)) {
			InfoString = _("Requirements not met");
		} else {
			InfoString = myPlayer.HoldItem.getName();
			InfoColor = myPlayer.HoldItem.getTextColor();
		}
	} else {
		if (pcursitem != -1)
			GetItemStr(Items[pcursitem]);
		else if (ObjectUnderCursor != nullptr)
			GetObjectStr(*ObjectUnderCursor);
		if (pcursmonst != -1) {
			if (leveltype != DTYPE_TOWN) {
				const Monster &monster = Monsters[pcursmonst];
				InfoColor = UiFlags::ColorWhite;
				InfoString = monster.name();
				if (monster.isUnique()) {
					InfoColor = UiFlags::ColorWhitegold;
					PrintUniqueHistory();
				} else {
					PrintMonstHistory(monster.type().type);
				}
			} else if (pcursitem == -1) {
				InfoString = std::string_view(Towners[pcursmonst].name);
			}
		}
		if (PlayerUnderCursor != nullptr) {
			InfoColor = UiFlags::ColorWhitegold;
			const auto &target = *PlayerUnderCursor;
			InfoString = std::string_view(target._pName);
			AddInfoBoxString(fmt::format(fmt::runtime(_("{:s}, Level: {:d}")), target.getClassName(), target.getCharacterLevel()));
			AddInfoBoxString(fmt::format(fmt::runtime(_("Hit Points {:d} of {:d}")), target._pHitPoints >> 6, target._pMaxHP >> 6));
		}
		if (PortraitIdUnderCursor != -1) {
			InfoColor = UiFlags::ColorWhitegold;
			auto &target = Players[PortraitIdUnderCursor];
			InfoString = std::string_view(target._pName);
			AddInfoBoxString(_("Right click to inspect"));
		}
	}
	if (!InfoString.empty())
		PrintInfo(out);
}

void DrawFloatingInfoBox(const Surface &out)
{
	if (pcursinvitem == -1 && pcursstashitem == StashStruct::EmptyCell) {
		FloatingInfoString = StringOrView {};
		InfoColor = UiFlags::ColorWhite;
	}

	if (!FloatingInfoString.empty())
		PrintFloatingInfo(out);
}

} // namespace devilution
