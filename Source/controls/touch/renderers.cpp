#include "controls/touch/renderers.h"

#include "control.h"
#include "cursor.h"
#include "diablo.h"
#include "doom.h"
#include "engine/events.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "init.h"
#include "inv.h"
#include "levels/gendung.h"
#include "minitext.h"
#include "panels/ui_panels.hpp"
#include "qol/stash.h"
#include "stores.h"
#include "towners.h"
#include "utils/sdl_compat.h"
#include "utils/sdl_geometry.h"
#include "utils/sdl_wrap.h"

namespace devilution {

namespace {

VirtualGamepadRenderer Renderer(&VirtualGamepadState);

VirtualGamepadButtonType GetAttackButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_ATTACKDOWN : GAMEPAD_ATTACK;
}

VirtualGamepadButtonType GetTalkButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_TALKDOWN : GAMEPAD_TALK;
}

VirtualGamepadButtonType GetItemButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_ITEMDOWN : GAMEPAD_ITEM;
}

VirtualGamepadButtonType GetObjectButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_OBJECTDOWN : GAMEPAD_OBJECT;
}

VirtualGamepadButtonType GetCastButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_CASTSPELLDOWN : GAMEPAD_CASTSPELL;
}

VirtualGamepadButtonType GetBackButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_BACKDOWN : GAMEPAD_BACK;
}

VirtualGamepadButtonType GetBlankButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_BLANKDOWN : GAMEPAD_BLANK;
}

VirtualGamepadButtonType GetPotionButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_POTIONDOWN : GAMEPAD_POTION;
}

VirtualGamepadButtonType GetApplyButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_APPLYDOWN : GAMEPAD_APPLY;
}

VirtualGamepadButtonType GetEquipButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_EQUIPDOWN : GAMEPAD_EQUIP;
}

VirtualGamepadButtonType GetDropButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_DROPDOWN : GAMEPAD_DROP;
}

VirtualGamepadButtonType GetStairsButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_STAIRSDOWN : GAMEPAD_STAIRS;
}

VirtualGamepadButtonType GetStandButtonType(bool isPressed)
{
	return isPressed ? GAMEPAD_STANDDOWN : GAMEPAD_STAND;
}

void LoadButtonArt(ButtonTexture *buttonArt)
{
	constexpr unsigned Sprites = 13;
	constexpr unsigned Frames = 2;
	buttonArt->surface.reset(LoadPNG("ui_art\\button.png"));
	if (buttonArt->surface == nullptr)
		return;

	buttonArt->numSprites = Sprites;
	buttonArt->numFrames = Frames;
}

void LoadPotionArt(ButtonTexture *potionArt)
{
	item_cursor_graphic potionGraphics[] {
		ICURS_POTION_OF_HEALING,
		ICURS_POTION_OF_MANA,
		ICURS_POTION_OF_REJUVENATION,
		ICURS_POTION_OF_FULL_HEALING,
		ICURS_POTION_OF_FULL_MANA,
		ICURS_POTION_OF_FULL_REJUVENATION,
		ICURS_ARENA_POTION,
		ICURS_SCROLL_OF
	};

	int potionFrame = static_cast<int>(CURSOR_FIRSTITEM) + static_cast<int>(ICURS_POTION_OF_HEALING);
	Size potionSize = GetInvItemSize(potionFrame);

	auto surface = SDLWrap::CreateRGBSurfaceWithFormat(
	    /*flags=*/0,
	    /*width=*/potionSize.width,
	    /*height=*/potionSize.height * sizeof(potionGraphics),
	    /*depth=*/8,
	    SDL_PIXELFORMAT_INDEX8);

	auto palette = SDLWrap::AllocPalette();
	if (SDLC_SetSurfaceAndPaletteColors(surface.get(), palette.get(), orig_palette.data(), 0, 256) < 0)
		ErrSdl();

	Uint32 bgColor = SDL_MapRGB(surface->format, orig_palette[1].r, orig_palette[1].g, orig_palette[1].b);
	if (SDL_FillRect(surface.get(), nullptr, bgColor) < 0)
		ErrSdl();
	if (SDL_SetColorKey(surface.get(), SDL_TRUE, bgColor) < 0)
		ErrSdl();

	Point position { 0, 0 };
	for (item_cursor_graphic graphic : potionGraphics) {
		const int cursorID = static_cast<int>(CURSOR_FIRSTITEM) + graphic;
		position.y += potionSize.height;
		ClxDraw(Surface(surface.get()), position, GetInvItemSprite(cursorID));
	}

	potionArt->numFrames = sizeof(potionGraphics);

	potionArt->surface.reset(SDL_ConvertSurfaceFormat(surface.get(), SDL_PIXELFORMAT_ARGB8888, 0));
}

bool InteractsWithCharButton(Point point)
{
	Player &myPlayer = *MyPlayer;
	if (myPlayer._pStatPts == 0)
		return false;
	for (auto attribute : enum_values<CharacterAttribute>()) {
		if (myPlayer.GetBaseAttributeValue(attribute) >= myPlayer.GetMaximumAttributeValue(attribute))
			continue;
		auto buttonId = static_cast<size_t>(attribute);
		Rectangle button = CharPanelButtonRect[buttonId];
		button.position = GetPanelPosition(UiPanels::Character, button.position);
		if (button.contains(point)) {
			return true;
		}
	}
	return false;
}

} // namespace

Size ButtonTexture::size() const
{
	int w, h;
	if (surface != nullptr) {
		w = surface->w;
		h = surface->h;
	} else {
		SDL_QueryTexture(texture.get(), /*format=*/nullptr, /*access=*/nullptr, &w, &h);
	}
	w /= numSprites;
	h /= numFrames;
	return Size { w, h };
}

void RenderVirtualGamepad(SDL_Renderer *renderer)
{
	if (!gbRunGame)
		return;

	RenderFunction renderFunction = [renderer](const ButtonTexture &art, SDL_Rect *src, SDL_Rect *dst) {
		if (art.texture == nullptr)
			return;

		if (SDL_RenderCopy(renderer, art.texture.get(), src, dst) <= -1)
			ErrSdl();
	};

	Renderer.Render(renderFunction);
}

void RenderVirtualGamepad(SDL_Surface *surface)
{
	if (!gbRunGame)
		return;

	RenderFunction renderFunction = [surface](const ButtonTexture &art, SDL_Rect *src, SDL_Rect *dst) {
		if (art.surface == nullptr)
			return;

		if (SDL_BlitScaled(art.surface.get(), src, surface, dst) <= -1)
			ErrSdl();
	};

	Renderer.Render(renderFunction);
}

void VirtualGamepadRenderer::LoadArt()
{
	menuPanelRenderer.LoadArt();
	directionPadRenderer.LoadArt();
	LoadButtonArt(&buttonArt);
	LoadPotionArt(&potionArt);
}

void VirtualMenuPanelRenderer::LoadArt()
{
	menuArt.surface.reset(LoadPNG("ui_art\\menu.png"));
	menuArtLevelUp.surface.reset(LoadPNG("ui_art\\menu-levelup.png"));
}

void VirtualDirectionPadRenderer::LoadArt()
{
	padArt.surface.reset(LoadPNG("ui_art\\directions.png"));
	knobArt.surface.reset(LoadPNG("ui_art\\directions2.png"));
}

void VirtualGamepadRenderer::Render(RenderFunction renderFunction)
{
	if (CurrentEventHandler == DisableInputEventHandler)
		return;

	primaryActionButtonRenderer.Render(renderFunction, buttonArt);
	secondaryActionButtonRenderer.Render(renderFunction, buttonArt);
	spellActionButtonRenderer.Render(renderFunction, buttonArt);
	cancelButtonRenderer.Render(renderFunction, buttonArt);
	healthButtonRenderer.Render(renderFunction, buttonArt);
	manaButtonRenderer.Render(renderFunction, buttonArt);

	healthButtonRenderer.RenderPotion(renderFunction, potionArt);
	manaButtonRenderer.RenderPotion(renderFunction, potionArt);

	if (leveltype != DTYPE_TOWN)
		standButtonRenderer.Render(renderFunction, buttonArt);
	directionPadRenderer.Render(renderFunction);
	menuPanelRenderer.Render(renderFunction);
}

void VirtualMenuPanelRenderer::Render(RenderFunction renderFunction)
{
	int x = virtualMenuPanel->area.position.x;
	int y = virtualMenuPanel->area.position.y;
	int width = virtualMenuPanel->area.size.width;
	int height = virtualMenuPanel->area.size.height;
	SDL_Rect rect { x, y, width, height };
	renderFunction(MyPlayer->_pStatPts == 0 ? menuArt : menuArtLevelUp, nullptr, &rect);
}

void VirtualDirectionPadRenderer::Render(RenderFunction renderFunction)
{
	RenderPad(renderFunction);
	RenderKnob(renderFunction);
}

void VirtualDirectionPadRenderer::RenderPad(RenderFunction renderFunction)
{
	auto center = virtualDirectionPad->area.position;
	auto radius = virtualDirectionPad->area.radius;
	int diameter = 2 * radius;

	int x = center.x - radius;
	int y = center.y - radius;
	int width = diameter;
	int height = diameter;
	SDL_Rect rect { x, y, width, height };
	renderFunction(padArt, nullptr, &rect);
}

void VirtualDirectionPadRenderer::RenderKnob(RenderFunction renderFunction)
{
	auto center = virtualDirectionPad->position;
	auto radius = virtualDirectionPad->area.radius / 3;
	int diameter = 2 * radius;

	int x = center.x - radius;
	int y = center.y - radius;
	int width = diameter;
	int height = diameter;
	SDL_Rect rect { x, y, width, height };
	renderFunction(knobArt, nullptr, &rect);
}

void VirtualPadButtonRenderer::Render(RenderFunction renderFunction, const ButtonTexture &buttonArt)
{
	if (!virtualPadButton->isUsable())
		return;

	VirtualGamepadButtonType buttonType = GetButtonType();
	Size size = buttonArt.size();
	const auto index = static_cast<unsigned>(buttonType);
	const int xOffset = size.width * (index / buttonArt.numFrames);
	const int yOffset = size.height * (index % buttonArt.numFrames);

	auto center = virtualPadButton->area.position;
	auto radius = virtualPadButton->area.radius;
	int diameter = 2 * radius;

	int x = center.x - radius;
	int y = center.y - radius;
	int width = diameter;
	int height = diameter;

	SDL_Rect src = MakeSdlRect(xOffset, yOffset, size.width, size.height);
	SDL_Rect dst = MakeSdlRect(x, y, width, height);
	renderFunction(buttonArt, &src, &dst);
}

void PotionButtonRenderer::RenderPotion(RenderFunction renderFunction, const ButtonTexture &potionArt)
{
	if (!virtualPadButton->isUsable())
		return;

	std::optional<VirtualGamepadPotionType> potionType = GetPotionType();
	if (potionType == std::nullopt)
		return;

	int frame = *potionType;
	Size size = potionArt.size();
	int offset = size.height * frame;

	auto center = virtualPadButton->area.position;
	auto radius = virtualPadButton->area.radius * 8 / 10;
	int diameter = 2 * radius;

	int x = center.x - radius;
	int y = center.y - radius;
	int width = diameter;
	int height = diameter;

	SDL_Rect src = MakeSdlRect(0, offset, size.width, size.height);
	SDL_Rect dst = MakeSdlRect(x, y, width, height);
	renderFunction(potionArt, &src, &dst);
}

std::optional<VirtualGamepadPotionType> PotionButtonRenderer::GetPotionType()
{
	for (const Item &item : InspectPlayer->SpdList) {
		if (item.isEmpty()) {
			continue;
		}

		if (potionType == BeltItemType::Healing) {
			if (item._iMiscId == IMISC_HEAL)
				return GAMEPAD_HEALING;
			if (item._iMiscId == IMISC_FULLHEAL)
				return GAMEPAD_FULL_HEALING;
			if (item.isScrollOf(SpellID::Healing))
				return GAMEPAD_SCROLL_OF_HEALING;
		}

		if (potionType == BeltItemType::Mana) {
			if (item._iMiscId == IMISC_MANA)
				return GAMEPAD_MANA;
			if (item._iMiscId == IMISC_FULLMANA)
				return GAMEPAD_FULL_MANA;
		}

		if (item._iMiscId == IMISC_REJUV)
			return GAMEPAD_REJUVENATION;
		if (item._iMiscId == IMISC_FULLREJUV)
			return GAMEPAD_FULL_REJUVENATION;
		if (item._iMiscId == IMISC_ARENAPOT && MyPlayer->isOnArenaLevel())
			return GAMEPAD_ARENA_POTION;
	}

	return std::nullopt;
}

VirtualGamepadButtonType StandButtonRenderer::GetButtonType()
{
	return GetStandButtonType(virtualPadButton->isHeld);
}

VirtualGamepadButtonType PrimaryActionButtonRenderer::GetButtonType()
{
	// NEED: Confirm surface
	if (qtextflag)
		return GetTalkButtonType(virtualPadButton->isHeld);
	if (CharFlag && InteractsWithCharButton(MousePosition))
		return GetApplyButtonType(virtualPadButton->isHeld);
	if (invflag)
		return GetInventoryButtonType();
	if (leveltype == DTYPE_TOWN)
		return GetTownButtonType();
	return GetDungeonButtonType();
}

VirtualGamepadButtonType PrimaryActionButtonRenderer::GetTownButtonType()
{
	if (IsPlayerInStore() || pcursmonst != -1)
		return GetTalkButtonType(virtualPadButton->isHeld);
	return GetBlankButtonType(virtualPadButton->isHeld);
}

VirtualGamepadButtonType PrimaryActionButtonRenderer::GetDungeonButtonType()
{
	if (pcursmonst != -1) {
		const Monster &monster = Monsters[pcursmonst];
		if (M_Talker(monster) || monster.talkMsg != TEXT_NONE)
			return GetTalkButtonType(virtualPadButton->isHeld);
	}
	return GetAttackButtonType(virtualPadButton->isHeld);
}

VirtualGamepadButtonType PrimaryActionButtonRenderer::GetInventoryButtonType()
{
	if (pcursinvitem != -1 || pcursstashitem != StashStruct::EmptyCell || pcurs > CURSOR_HAND)
		return GetItemButtonType(virtualPadButton->isHeld);
	return GetBlankButtonType(virtualPadButton->isHeld);
}

extern int pcurstrig;
extern Missile *pcursmissile;
extern quest_id pcursquest;

VirtualGamepadButtonType SecondaryActionButtonRenderer::GetButtonType()
{
	if (pcursmissile != nullptr || pcurstrig != -1 || pcursquest != Q_INVALID) {
		return GetStairsButtonType(virtualPadButton->isHeld);
	}
	if (InGameMenu() || QuestLogIsOpen || SpellbookFlag)
		return GetBlankButtonType(virtualPadButton->isHeld);
	if (ObjectUnderCursor != nullptr)
		return GetObjectButtonType(virtualPadButton->isHeld);
	if (pcursitem != -1)
		return GetItemButtonType(virtualPadButton->isHeld);
	if (invflag) {
		if (pcurs > CURSOR_HAND && pcurs < CURSOR_FIRSTITEM)
			return GetApplyButtonType(virtualPadButton->isHeld);

		if (pcursinvitem != -1) {
			Item &item = GetInventoryItem(*MyPlayer, pcursinvitem);
			if (!item.isScroll() || !TargetsMonster(item._iSpell)) {
				if (!item.isEquipment()) {
					return GetApplyButtonType(virtualPadButton->isHeld);
				}
			}
		}
	}

	return GetBlankButtonType(virtualPadButton->isHeld);
}

VirtualGamepadButtonType SpellActionButtonRenderer::GetButtonType()
{
	if (!MyPlayer->HoldItem.isEmpty())
		return GetDropButtonType(virtualPadButton->isHeld);

	if (invflag && pcursinvitem != -1 && pcurs == CURSOR_HAND) {
		return GetEquipButtonType(virtualPadButton->isHeld);
	}

	if (!invflag && !InGameMenu() && !QuestLogIsOpen && !SpellbookFlag)
		return GetCastButtonType(virtualPadButton->isHeld);
	return GetBlankButtonType(virtualPadButton->isHeld);
}

VirtualGamepadButtonType CancelButtonRenderer::GetButtonType()
{
	if (InGameMenu())
		return GetBackButtonType(virtualPadButton->isHeld);
	if (DoomFlag || invflag || SpellbookFlag || QuestLogIsOpen || CharFlag)
		return GetBackButtonType(virtualPadButton->isHeld);
	return GetBlankButtonType(virtualPadButton->isHeld);
}

VirtualGamepadButtonType PotionButtonRenderer::GetButtonType()
{
	return GetPotionButtonType(virtualPadButton->isHeld);
}

void VirtualGamepadRenderer::UnloadArt()
{
	menuPanelRenderer.UnloadArt();
	directionPadRenderer.UnloadArt();
	buttonArt.clearSurface();
	potionArt.clearSurface();
}

void VirtualMenuPanelRenderer::UnloadArt()
{
	menuArt.clearSurface();
	menuArtLevelUp.clearSurface();
}

void VirtualDirectionPadRenderer::UnloadArt()
{
	padArt.clearSurface();
	knobArt.clearSurface();
}

void InitVirtualGamepadGFX()
{
	Renderer.LoadArt();
}

void FreeVirtualGamepadGFX()
{
	Renderer.UnloadArt();
}

void VirtualGamepadRenderer::createTextures(SDL_Renderer &renderer)
{
	menuPanelRenderer.createTextures(renderer);
	directionPadRenderer.createTextures(renderer);
	if (buttonArt.surface != nullptr) {
		buttonArt.texture.reset(SDL_CreateTextureFromSurface(&renderer, buttonArt.surface.get()));
		buttonArt.surface = nullptr;
	}
	if (potionArt.surface != nullptr) {
		potionArt.texture.reset(SDL_CreateTextureFromSurface(&renderer, potionArt.surface.get()));
		potionArt.surface = nullptr;
	}
}

void VirtualMenuPanelRenderer::createTextures(SDL_Renderer &renderer)
{
	if (menuArt.surface != nullptr) {
		menuArt.texture.reset(SDL_CreateTextureFromSurface(&renderer, menuArt.surface.get()));
		menuArt.surface = nullptr;
	}
	if (menuArtLevelUp.surface != nullptr) {
		menuArtLevelUp.texture.reset(SDL_CreateTextureFromSurface(&renderer, menuArtLevelUp.surface.get()));
		menuArtLevelUp.surface = nullptr;
	}
}

void VirtualDirectionPadRenderer::createTextures(SDL_Renderer &renderer)
{
	if (padArt.surface != nullptr) {
		padArt.texture.reset(SDL_CreateTextureFromSurface(&renderer, padArt.surface.get()));
		padArt.surface = nullptr;
	}
	if (knobArt.surface != nullptr) {
		knobArt.texture.reset(SDL_CreateTextureFromSurface(&renderer, knobArt.surface.get()));
		knobArt.surface = nullptr;
	}
}

void VirtualGamepadRenderer::destroyTextures()
{
	menuPanelRenderer.destroyTextures();
	directionPadRenderer.destroyTextures();
	buttonArt.destroyTexture();
	potionArt.destroyTexture();
}

void VirtualMenuPanelRenderer::destroyTextures()
{
	menuArt.destroyTexture();
	menuArtLevelUp.destroyTexture();
}

void VirtualDirectionPadRenderer::destroyTextures()
{
	padArt.destroyTexture();
	knobArt.destroyTexture();
}

void InitVirtualGamepadTextures(SDL_Renderer &renderer)
{
	Renderer.createTextures(renderer);
}

void FreeVirtualGamepadTextures()
{
	Renderer.destroyTextures();
}

} // namespace devilution
