/**
 * @file monhealthbar.cpp
 *
 * Adds monster health bar QoL feature
 */
#include "monhealthbar.h"

#include <cstdint>

#include <fmt/format.h>

#include "control.h"
#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "options.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {

OptionalOwnedClxSpriteList healthBox;
OptionalOwnedClxSpriteList resistance;
OptionalOwnedClxSpriteList health;
OptionalOwnedClxSpriteList healthBlue;
OptionalOwnedClxSpriteList playerExpTags;

} // namespace

void InitMonsterHealthBar()
{
	if (!*sgOptions.Gameplay.enemyHealthBar)
		return;

	healthBox = LoadClx("data\\healthbox.clx");
	health = LoadClx("data\\health.clx");
	resistance = LoadClx("data\\resistance.clx");
	playerExpTags = LoadClx("data\\monstertags.clx");

	std::array<uint8_t, 256> healthBlueTrn;
	healthBlueTrn[234] = 185;
	healthBlueTrn[235] = 186;
	healthBlueTrn[236] = 187;
	healthBlue = health->clone();
	ClxApplyTrans(*healthBlue, healthBlueTrn.data());
}

void FreeMonsterHealthBar()
{
	healthBlue = std::nullopt;
	playerExpTags = std::nullopt;
	resistance = std::nullopt;
	health = std::nullopt;
	healthBox = std::nullopt;
}

void DrawMonsterHealthBar(const Surface &out)
{
	if (!*sgOptions.Gameplay.enemyHealthBar)
		return;

	if (leveltype == DTYPE_TOWN)
		return;
	if (pcursmonst == -1)
		return;

	const Monster &monster = Monsters[pcursmonst];

	const int width = (*healthBox)[0].width();
	const int barWidth = (*health)[0].width();
	const int height = (*healthBox)[0].height();
	Point position = { (gnScreenWidth - width) / 2, 18 };

	if (CanPanelsCoverView()) {
		if (IsRightPanelOpen())
			position.x -= SidePanelSize.width / 2;
		if (IsLeftPanelOpen())
			position.x += SidePanelSize.width / 2;
	}

	const int border = 3;

	int multiplier = 0;
	int currLife = monster.hitPoints;
	// lifestealing monsters can reach HP exceeding their max
	if (monster.hitPoints > monster.maxHitPoints) {
		multiplier = monster.hitPoints / monster.maxHitPoints;
		currLife = monster.hitPoints - monster.maxHitPoints * multiplier;
		if (currLife == 0 && multiplier > 0) {
			multiplier--;
			currLife = monster.maxHitPoints;
		}
	}

	RenderClxSprite(out, (*healthBox)[0], position);
	DrawHalfTransparentRectTo(out, position.x + border, position.y + border, width - (border * 2), height - (border * 2));
	int barProgress = (barWidth * currLife) / monster.maxHitPoints;
	if (barProgress != 0) {
		RenderClxSprite(
		    out.subregion(position.x + border + 1, position.y + border + 1, barProgress, height - (border * 2) - 2),
		    (*(multiplier > 0 ? healthBlue : health))[0], { 0, 0 });
	}

	constexpr auto GetBorderColor = [](MonsterClass monsterClass) {
		switch (monsterClass) {
		case MonsterClass::Undead:
			return 248;

		case MonsterClass::Demon:
			return 232;

		case MonsterClass::Animal:
			return 150;

		default:
			app_fatal(StrCat("Invalid monster class: ", static_cast<int>(monsterClass)));
		}
	};

	if (*sgOptions.Gameplay.showMonsterType) {
		Uint8 borderColor = GetBorderColor(monster.data().monsterClass);
		int borderWidth = width - (border * 2);
		UnsafeDrawHorizontalLine(out, { position.x + border, position.y + border }, borderWidth, borderColor);
		UnsafeDrawHorizontalLine(out, { position.x + border, position.y + height - border - 1 }, borderWidth, borderColor);
		int borderHeight = height - (border * 2) - 2;
		UnsafeDrawVerticalLine(out, { position.x + border, position.y + border + 1 }, borderHeight, borderColor);
		UnsafeDrawVerticalLine(out, { position.x + width - border - 1, position.y + border + 1 }, borderHeight, borderColor);
	}

	UiFlags style = UiFlags::AlignCenter | UiFlags::VerticalCenter;
	DrawString(out, monster.name(), { position + Displacement { -1, 1 }, { width, height } }, { style | UiFlags::ColorBlack });
	if (monster.isUnique())
		style |= UiFlags::ColorWhitegold;
	else if (monster.leader != Monster::NoLeader)
		style |= UiFlags::ColorBlue;
	else
		style |= UiFlags::ColorWhite;
	DrawString(out, monster.name(), { position, { width, height } }, { style });

	if (multiplier > 0)
		DrawString(out, StrCat("x", multiplier), { position, { width - 2, height } }, { UiFlags::ColorWhite | UiFlags::AlignRight | UiFlags::VerticalCenter });
	if (monster.isUnique() || MonsterKillCounts[monster.type().type] >= 15) {
		monster_resistance immunes[] = { IMMUNE_MAGIC, IMMUNE_FIRE, IMMUNE_LIGHTNING };
		monster_resistance resists[] = { RESIST_MAGIC, RESIST_FIRE, RESIST_LIGHTNING };

		int resOffset = 5;
		for (size_t i = 0; i < 3; i++) {
			if ((monster.resistance & immunes[i]) != 0) {
				RenderClxSprite(out, (*resistance)[i * 2 + 1], position + Displacement { resOffset, height - 6 });
				resOffset += (*resistance)[0].width() + 2;
			} else if ((monster.resistance & resists[i]) != 0) {
				RenderClxSprite(out, (*resistance)[i * 2], position + Displacement { resOffset, height - 6 });
				resOffset += (*resistance)[0].width() + 2;
			}
		}
	}

	if (Players.size() > 1) {
		int tagOffset = 5;
		for (size_t i = 0; i < Players.size(); i++) {
			if (((1U << i) & monster.whoHit) != 0) {
				RenderClxSprite(out, (*playerExpTags)[i + 1], position + Displacement { tagOffset, height - 31 });
			} else if (Players[i].plractive) {
				RenderClxSprite(out, (*playerExpTags)[0], position + Displacement { tagOffset, height - 31 });
			}
			tagOffset += (*playerExpTags)[0].width();
		}
	}
}

} // namespace devilution
