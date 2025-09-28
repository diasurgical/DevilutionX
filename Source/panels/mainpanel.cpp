#include "panels/mainpanel.hpp"

#include <cstdint>

#include "control.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "utils/display.h"
#include "utils/language.h"
#include "utils/sdl_compat.h"
#include "utils/sdl_geometry.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/surface_to_clx.hpp"

namespace devilution {

OptionalOwnedClxSpriteList PanelButtonDown;
OptionalOwnedClxSpriteList TalkButton;

namespace {

OptionalOwnedClxSpriteList PanelButton;
OptionalOwnedClxSpriteList PanelButtonGrime;
OptionalOwnedClxSpriteList PanelButtonDownGrime;

void DrawButtonText(const Surface &out, string_view text, Rectangle placement, UiFlags style, int spacing = 1)
{
	DrawString(out, text, { placement.position + Displacement { 0, 1 }, placement.size }, { UiFlags::AlignCenter | UiFlags::KerningFitSpacing | UiFlags::ColorBlack, spacing });
	DrawString(out, text, placement, { UiFlags::AlignCenter | UiFlags::KerningFitSpacing | style, spacing });
}

void DrawButtonOnPanel(Point position, string_view text, int frame)
{
	RenderClxSprite(*pBtmBuff, (*PanelButton)[frame], position);
	int spacing = 2;
	int width = std::min<int>(GetLineWidth(text, GameFont12, spacing), (*PanelButton)[0].width());
	if (width > 38) {
		spacing = 1;
		width = std::min<int>(GetLineWidth(text, GameFont12, spacing), (*PanelButton)[0].width());
	}
	RenderClxSprite(pBtmBuff->subregion(position.x + ((*PanelButton)[0].width() - width) / 2, position.y + 7, width, pBtmBuff->h() - 7), (*PanelButtonGrime)[frame], { 0, 0 });
	DrawButtonText(*pBtmBuff, text, { position, { (*PanelButton)[0].width(), 0 } }, UiFlags::ColorButtonface, spacing);
}

void RenderMainButton(const Surface &out, int buttonId, string_view text, int frame)
{
	Point panelPosition { PanBtnPos[buttonId].x + 4, PanBtnPos[buttonId].y + 17 };
	DrawButtonOnPanel(panelPosition, text, frame);
	if (IsChatAvailable())
		DrawButtonOnPanel(panelPosition + Displacement { 0, GetMainPanel().size.height + 16 }, text, frame);

	Point position { 0, 19 * buttonId };
	int spacing = 2;
	int width = std::min<int>(GetLineWidth(text, GameFont12, spacing), (*PanelButton)[0].width());
	if (width > 38) {
		spacing = 1;
		width = std::min<int>(GetLineWidth(text, GameFont12, spacing), (*PanelButton)[0].width());
	}
	RenderClxSprite(out.subregion(position.x + ((*PanelButton)[0].width() - width) / 2, position.y + 9, width, out.h() - position.y - 9), (*PanelButtonDownGrime)[frame], { 0, 0 });
	DrawButtonText(out, text, { position + Displacement { 0, 2 }, { out.w(), 0 } }, UiFlags::ColorButtonpushed, spacing);
}

} // namespace

void LoadMainPanel()
{
	std::optional<OwnedSurface> out;
	constexpr uint16_t NumButtonSprites = 6;
	{
		OptionalOwnedClxSpriteList background = LoadClx("data\\panel8bucp.clx");
		out.emplace((*background)[0].width(), (*background)[0].height() * NumButtonSprites);
		int y = 0;
		for (ClxSprite sprite : ClxSpriteList(*background)) {
			RenderClxSprite(*out, sprite, { 0, y });
			y += sprite.height();
		}
	}

	PanelButton = LoadOptionalClx("data\\panel8buc.clx");
	PanelButtonGrime = LoadOptionalClx("data\\dirtybuc.clx");
	PanelButtonDownGrime = LoadOptionalClx("data\\dirtybucp.clx");

	RenderMainButton(*out, 0, _("char"), 0);
	RenderMainButton(*out, 1, _("quests"), 1);
	RenderMainButton(*out, 2, _("map"), 1);
	RenderMainButton(*out, 3, _("menu"), 0);
	RenderMainButton(*out, 4, _("inv"), 1);
	RenderMainButton(*out, 5, _("spells"), 0);
	PanelButtonDown = SurfaceToClx(*out, NumButtonSprites);
	out = std::nullopt;

	if (IsChatAvailable()) {
		OptionalOwnedClxSpriteList talkButton = LoadClx("data\\talkbutton.clx");
		const int talkButtonWidth = (*talkButton)[0].width();

		constexpr size_t NumOtherPlayers = 3;
		// Render the unpressed voice buttons to pBtmBuff.
		string_view text = _("voice");
		const int textWidth = GetLineWidth(text, GameFont12, 1);
		for (size_t i = 0; i < NumOtherPlayers; ++i) {
			Point position { 176, static_cast<int>(GetMainPanel().size.height + 101 + 18 * i) };
			RenderClxSprite(*pBtmBuff, (*talkButton)[0], position);
			int width = std::min<int>(textWidth, (*PanelButton)[0].width());
			RenderClxSprite(pBtmBuff->subregion(position.x + (talkButtonWidth - width) / 2, position.y + 6, width, 9), (*PanelButtonGrime)[1], { 0, 0 });
			DrawButtonText(*pBtmBuff, text, { position, { talkButtonWidth, 0 } }, UiFlags::ColorButtonface);
		}

		const int talkButtonHeight = (*talkButton)[0].height();
		constexpr uint16_t NumTalkButtonSprites = 3;
		OwnedSurface talkSurface(talkButtonWidth, talkButtonHeight * NumTalkButtonSprites);

		// Prerender translated versions of the other button states for voice buttons
		RenderClxSprite(talkSurface, (*talkButton)[0], { 0, 0 });                    // background for unpressed mute button
		RenderClxSprite(talkSurface, (*talkButton)[1], { 0, talkButtonHeight });     // background for pressed mute button
		RenderClxSprite(talkSurface, (*talkButton)[1], { 0, talkButtonHeight * 2 }); // background for pressed voice button

		talkButton = std::nullopt;

		int muteWidth = GetLineWidth(_("mute"), GameFont12, 2);
		RenderClxSprite(talkSurface.subregion((talkButtonWidth - muteWidth) / 2, 6, muteWidth, 9), (*PanelButtonGrime)[1], { 0, 0 });
		DrawButtonText(talkSurface, _("mute"), { { 0, 0 }, { talkButtonWidth, 0 } }, UiFlags::ColorButtonface);
		RenderClxSprite(talkSurface.subregion((talkButtonWidth - muteWidth) / 2, 23, muteWidth, 9), (*PanelButtonGrime)[1], { 0, 0 });
		DrawButtonText(talkSurface, _("mute"), { { 0, 17 }, { talkButtonWidth, 0 } }, UiFlags::ColorButtonpushed);
		int voiceWidth = GetLineWidth(_("voice"), GameFont12, 2);
		RenderClxSprite(talkSurface.subregion((talkButtonWidth - voiceWidth) / 2, 39, voiceWidth, 9), (*PanelButtonGrime)[1], { 0, 0 });
		DrawButtonText(talkSurface, _("voice"), { { 0, 33 }, { talkButtonWidth, 0 } }, UiFlags::ColorButtonpushed);
		TalkButton = SurfaceToClx(talkSurface, NumTalkButtonSprites);
	}

	PanelButtonDownGrime = std::nullopt;
	PanelButtonGrime = std::nullopt;
	PanelButton = std::nullopt;
}

void FreeMainPanel()
{
	TalkButton = std::nullopt;
	PanelButtonDown = std::nullopt;
}

} // namespace devilution
