#include "floatingnumbers.h"

#include <cstdint>
#include <ctime>
#include <deque>
#include <fmt/format.h>
#include <string>

#include "engine/render/text_render.hpp"
#include "options.h"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

struct FloatingNumber {
	Point startPos;
	Displacement startOffset;
	Displacement endOffset;
	std::string text;
	uint32_t time;
	uint32_t lastMerge;
	UiFlags style;
	DamageType type;
	int value;
	int index;
	bool reverseDirection;
};

std::deque<FloatingNumber> FloatingQueue;

void ClearExpiredNumbers()
{
	while (!FloatingQueue.empty()) {
		FloatingNumber &num = FloatingQueue.front();
		if (num.time > SDL_GetTicks())
			break;

		FloatingQueue.pop_front();
	}
}

GameFontTables GetGameFontSizeByDamage(int value)
{
	value >>= 6;
	if (value >= 300)
		return GameFont30;
	if (value >= 100)
		return GameFont24;
	return GameFont12;
}

UiFlags GetFontSizeByDamage(int value)
{
	value >>= 6;
	if (value >= 300)
		return UiFlags::FontSize30;
	if (value >= 100)
		return UiFlags::FontSize24;
	return UiFlags::FontSize12;
}

void UpdateFloatingData(FloatingNumber &num)
{
	if (num.value > 0 && num.value < 64) {
		num.text = fmt::format("{:.2f}", num.value / 64.0);
	} else {
		num.text = StrCat(num.value >> 6);
	}

	num.style &= ~(UiFlags::FontSize12 | UiFlags::FontSize24 | UiFlags::FontSize30);
	num.style |= GetFontSizeByDamage(num.value);

	switch (num.type) {
	case DamageType::Physical:
		num.style |= UiFlags::ColorGold;
		break;
	case DamageType::Fire:
		num.style |= UiFlags::ColorUiSilver; // UiSilver appears dark red ingame
		break;
	case DamageType::Lightning:
		num.style |= UiFlags::ColorBlue;
		break;
	case DamageType::Magic:
		num.style |= UiFlags::ColorOrange;
		break;
	case DamageType::Acid:
		num.style |= UiFlags::ColorYellow;
		break;
	}
}

void AddFloatingNumber(Point pos, Displacement offset, DamageType type, int value, int index, bool damageToPlayer)
{
	// 45 deg angles to avoid jitter caused by px alignment
	Displacement goodAngles[] = {
		{ 0, -140 },
		{ 100, -100 },
		{ -100, -100 },
	};

	Displacement endOffset;
	if (*sgOptions.Gameplay.enableFloatingNumbers == FloatingNumbers::Random) {
		endOffset = goodAngles[rand() % 3];
	} else if (*sgOptions.Gameplay.enableFloatingNumbers == FloatingNumbers::Vertical) {
		endOffset = goodAngles[0];
	}

	if (damageToPlayer)
		endOffset = -endOffset;

	for (auto &num : FloatingQueue) {
		if (num.reverseDirection == damageToPlayer && num.type == type && num.index == index && (SDL_GetTicks() - static_cast<int>(num.lastMerge)) <= 100) {
			num.value += value;
			num.lastMerge = SDL_GetTicks();
			UpdateFloatingData(num);
			return;
		}
	}
	FloatingNumber num {
		pos, offset, endOffset, "", SDL_GetTicks() + 2500, SDL_GetTicks(), UiFlags::Outlined, type, value, index, damageToPlayer
	};
	UpdateFloatingData(num);
	FloatingQueue.push_back(num);
}

} // namespace

void AddFloatingNumber(DamageType damageType, const Monster &monster, int damage)
{
	if (*sgOptions.Gameplay.enableFloatingNumbers == FloatingNumbers::Off)
		return;

	Displacement offset = {};
	if (monster.isWalking()) {
		offset = GetOffsetForWalking(monster.animInfo, monster.direction);
		if (monster.mode == MonsterMode::MoveSideways) {
			if (monster.direction == Direction::West)
				offset -= Displacement { 64, 0 };
			else
				offset += Displacement { 64, 0 };
		}
	}
	if (monster.animInfo.sprites) {
		const ClxSprite sprite = monster.animInfo.currentSprite();
		offset.deltaY -= sprite.height() / 2;
	}

	AddFloatingNumber(monster.position.tile, offset, damageType, damage, monster.getId(), false);
}

void AddFloatingNumber(DamageType damageType, const Player &player, int damage)
{
	if (*sgOptions.Gameplay.enableFloatingNumbers == FloatingNumbers::Off)
		return;

	Displacement offset = {};
	if (player.isWalking()) {
		offset = GetOffsetForWalking(player.AnimInfo, player._pdir);
		if (player._pmode == PM_WALK_SIDEWAYS) {
			if (player._pdir == Direction::West)
				offset -= Displacement { 64, 0 };
			else
				offset += Displacement { 64, 0 };
		}
	}

	AddFloatingNumber(player.position.tile, offset, damageType, damage, player.getId(), true);
}

void DrawFloatingNumbers(const Surface &out, Point viewPosition, Displacement offset)
{
	if (*sgOptions.Gameplay.enableFloatingNumbers == FloatingNumbers::Off)
		return;

	for (auto &floatingNum : FloatingQueue) {
		Displacement worldOffset = viewPosition - floatingNum.startPos;
		worldOffset = worldOffset.worldToScreen() + offset + Displacement { TILE_WIDTH / 2, -TILE_HEIGHT / 2 } + floatingNum.startOffset;

		if (*sgOptions.Graphics.zoom) {
			worldOffset *= 2;
		}

		Point screenPosition { worldOffset.deltaX, worldOffset.deltaY };

		int lineWidth = GetLineWidth(floatingNum.text, GetGameFontSizeByDamage(floatingNum.value));
		screenPosition.x -= lineWidth / 2;
		uint32_t timeLeft = floatingNum.time - SDL_GetTicks();
		float mul = 1 - (timeLeft / 2500.0f);
		screenPosition += floatingNum.endOffset * mul;

		DrawString(out, floatingNum.text, Rectangle { screenPosition, { lineWidth, 0 } }, { floatingNum.style });
	}

	ClearExpiredNumbers();
}

void ClearFloatingNumbers()
{
	srand(time(nullptr));

	FloatingQueue.clear();
}

} // namespace devilution
