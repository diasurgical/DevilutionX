#include "control.hpp"

#include "DiabloUI/text_input.hpp"
#include "engine/render/clx_render.hpp"
#include "inv.h"
#include "utils/display.h"
#include "utils/format_int.hpp"
#include "utils/log.hpp"
#include "utils/sdl_compat.h"

namespace devilution {

bool DropGoldFlag;
TextInputCursorState GoldDropCursor;
char GoldDropText[21];

namespace {

int8_t GoldDropInvIndex;
std::optional<NumberInputState> GoldDropInputState;

void RemoveGold(Player &player, int goldIndex, int amount)
{
	const int gi = goldIndex - INVITEM_INV_FIRST;
	player.InvList[gi]._ivalue -= amount;
	if (player.InvList[gi]._ivalue > 0) {
		SetPlrHandGoldCurs(player.InvList[gi]);
		NetSyncInvItem(player, gi);
	} else {
		player.RemoveInvItem(gi);
	}

	MakeGoldStack(player.HoldItem, amount);
	NewCursor(player.HoldItem);

	player._pGold = CalculateGold(player);
}

int GetGoldDropMax()
{
	return GoldDropInputState->max();
}

} // namespace

void DrawGoldSplit(const Surface &out)
{
	const int dialogX = 30;

	ClxDraw(out, GetPanelPosition(UiPanels::Inventory, { dialogX, 178 }), (*GoldBoxBuffer)[0]);

	const std::string_view amountText = GoldDropText;
	const TextInputCursorState &cursor = GoldDropCursor;
	const int max = GetGoldDropMax();

	const std::string description = fmt::format(
	    fmt::runtime(ngettext(
	        /* TRANSLATORS: {:s} is a number with separators. Dialog is shown when splitting a stash of Gold.*/
	        "You have {:s} gold piece. How many do you want to remove?",
	        "You have {:s} gold pieces. How many do you want to remove?",
	        max)),
	    FormatInteger(max));

	// Pre-wrap the string at spaces, otherwise DrawString would hard wrap in the middle of words
	const std::string wrapped = WordWrapString(description, 200);

	// The split gold dialog is roughly 4 lines high, but we need at least one line for the player to input an amount.
	// Using a clipping region 50 units high (approx 3 lines with a lineheight of 17) to ensure there is enough room left
	//  for the text entered by the player.
	DrawString(out, wrapped, { GetPanelPosition(UiPanels::Inventory, { dialogX + 31, 75 }), { 200, 50 } },
	    { .flags = UiFlags::ColorWhitegold | UiFlags::AlignCenter, .lineHeight = 17 });

	// Even a ten digit amount of gold only takes up about half a line. There's no need to wrap or clip text here so we
	// use the Point form of DrawString.
	DrawString(out, amountText, GetPanelPosition(UiPanels::Inventory, { dialogX + 37, 128 }),
	    {
	        .flags = UiFlags::ColorWhite | UiFlags::PentaCursor,
	        .cursorPosition = static_cast<int>(cursor.position),
	        .highlightRange = { static_cast<int>(cursor.selection.begin), static_cast<int>(cursor.selection.end) },
	    });
}

void control_drop_gold(SDL_Keycode vkey)
{
	Player &myPlayer = *MyPlayer;

	if (myPlayer.hasNoLife()) {
		CloseGoldDrop();
		return;
	}

	switch (vkey) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (const int value = GoldDropInputState->value(); value != 0) {
			RemoveGold(myPlayer, GoldDropInvIndex, value);
		}
		CloseGoldDrop();
		break;
	case SDLK_ESCAPE:
		CloseGoldDrop();
		break;
	default:
		break;
	}
}

void OpenGoldDrop(int8_t invIndex, int max)
{
	DropGoldFlag = true;
	GoldDropInvIndex = invIndex;
	GoldDropText[0] = '\0';
	GoldDropInputState.emplace(NumberInputState::Options {
	    .textOptions {
	        .value = GoldDropText,
	        .cursor = &GoldDropCursor,
	        .maxLength = sizeof(GoldDropText) - 1,
	    },
	    .min = 0,
	    .max = max,
	});
	SDLC_StartTextInput(ghMainWnd);
}

void CloseGoldDrop()
{
	if (!DropGoldFlag)
		return;
	SDLC_StopTextInput(ghMainWnd);
	DropGoldFlag = false;
	GoldDropInputState = std::nullopt;
	GoldDropInvIndex = 0;
}

bool HandleGoldDropTextInputEvent(const SDL_Event &event)
{
	return HandleInputEvent(event, GoldDropInputState);
}

} // namespace devilution
