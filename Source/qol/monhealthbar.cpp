/**
 * @file monhealthbar.cpp
 *
 * Adds monster health bar QoL feature
 */
#include "monhealthbar.h"

#include <cstdint>

#include <fmt/format.h>

#include "control/control.hpp"
#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/renderer.h"
#include "game_mode.hpp"
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

void OptionEnemyHealthBarChanged()
{
	if (!gbRunGame)
		return;
	if (*GetOptions().Gameplay.enemyHealthBar)
		InitMonsterHealthBar();
	else
		FreeMonsterHealthBar();
}

const auto OptionChangeHandler = (GetOptions().Gameplay.enemyHealthBar.SetValueChangedCallback(OptionEnemyHealthBarChanged), true);

} // namespace

void InitMonsterHealthBar()
{
	if (!*GetOptions().Gameplay.enemyHealthBar)
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

void DrawMonsterHealthBar()
{
	if (!*GetOptions().Gameplay.enemyHealthBar)
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

	GetRenderer().RenderClx(position, (*healthBox)[0]);
	GetRenderer().DrawBlendedRect(position.x + border, position.y + border, width - (border * 2), height - (border * 2));
	const int barProgress = (barWidth * currLife) / monster.maxHitPoints;
	if (barProgress != 0) {
		GetRenderer().SetClipRegion(position.x + border + 1, position.y + border + 1, barProgress, height - (border * 2) - 2);
		GetRenderer().RenderClx({ position.x + border + 1, position.y + border + 1 }, (*(multiplier > 0 ? healthBlue : health))[0]);
		GetRenderer().ClearClipRegion();
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

	if (*GetOptions().Gameplay.showMonsterType) {
		const Uint8 borderColor = GetBorderColor(monster.data().monsterClass);
		const int borderWidth = width - (border * 2);
		GetRenderer().DrawHorizontalLine({ position.x + border, position.y + border }, borderWidth, borderColor);
		GetRenderer().DrawHorizontalLine({ position.x + border, position.y + height - border - 1 }, borderWidth, borderColor);
		const int borderHeight = height - (border * 2) - 2;
		GetRenderer().DrawVerticalLine({ position.x + border, position.y + border + 1 }, borderHeight, borderColor);
		GetRenderer().DrawVerticalLine({ position.x + width - border - 1, position.y + border + 1 }, borderHeight, borderColor);
	}

	UiFlags style = UiFlags::AlignCenter | UiFlags::VerticalCenter;
	DrawString(monster.name(), { position + Displacement { -1, 1 }, { width, height } },
	    { .flags = style | UiFlags::ColorBlack });
	if (monster.isUnique())
		style |= UiFlags::ColorWhitegold;
	else if (monster.leader != Monster::NoLeader)
		style |= UiFlags::ColorBlue;
	else
		style |= UiFlags::ColorWhite;
	DrawString(monster.name(), { position, { width, height } },
	    { .flags = style });

	if (multiplier > 0)
		DrawString(StrCat("x", multiplier), { position, { width - 2, height } },
		    { .flags = UiFlags::ColorWhite | UiFlags::AlignRight | UiFlags::VerticalCenter });
	if (monster.isUnique() || MonsterKillCounts[monster.type().type] >= 15) {
		const monster_resistance immunes[] = { IMMUNE_MAGIC, IMMUNE_FIRE, IMMUNE_LIGHTNING };
		const monster_resistance resists[] = { RESIST_MAGIC, RESIST_FIRE, RESIST_LIGHTNING };

		int resOffset = 5;
		for (size_t i = 0; i < 3; i++) {
			if ((monster.resistance & immunes[i]) != 0) {
				GetRenderer().RenderClx(position + Displacement { resOffset, height - 6 }, (*resistance)[(i * 2) + 1]);
				resOffset += (*resistance)[0].width() + 2;
			} else if ((monster.resistance & resists[i]) != 0) {
				GetRenderer().RenderClx(position + Displacement { resOffset, height - 6 }, (*resistance)[i * 2]);
				resOffset += (*resistance)[0].width() + 2;
			}
		}
	}

	if (Players.size() > 1) {
		int tagOffset = 5;
		for (size_t i = 0; i < Players.size(); i++) {
			if (((1U << i) & monster.whoHit) != 0) {
				GetRenderer().RenderClx(position + Displacement { tagOffset, height - 31 }, (*playerExpTags)[i + 1]);
			} else if (Players[i].plractive) {
				GetRenderer().RenderClx(position + Displacement { tagOffset, height - 31 }, (*playerExpTags)[0]);
			}
			tagOffset += (*playerExpTags)[0].width();
		}
	}
}

} // namespace devilution
