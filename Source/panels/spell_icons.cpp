#include "panels/spell_icons.hpp"

#include <cstdint>

#include "engine.h"
#include "engine/load_cel.hpp"
#include "engine/load_clx.hpp"
#include "engine/palette.h"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "init.h"
#include "utils/stdcompat/optional.hpp"

namespace devilution {

namespace {

#ifdef UNPACKED_MPQS
OptionalOwnedClxSpriteList LargeSpellIconsBackground;
OptionalOwnedClxSpriteList SmallSpellIconsBackground;
#endif

OptionalOwnedClxSpriteList SmallSpellIcons;
OptionalOwnedClxSpriteList LargeSpellIcons;

uint8_t SplTransTbl[256];

/** Maps from SpellID to spelicon.cel frame number. */
const uint8_t SpellITbl[] = {
	26,
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	27,
	12,
	11,
	17,
	15,
	13,
	17,
	18,
	10,
	19,
	14,
	20,
	22,
	23,
	24,
	21,
	25,
	28,
	36,
	37,
	38,
	41,
	40,
	39,
	9,
	35,
	29,
	50,
	50,
	49,
	45,
	46,
	42,
	44,
	47,
	48,
	43,
	34,
	34,
	34,
	34,
	34,
};

} // namespace

void LoadLargeSpellIcons()
{
	if (!gbIsHellfire) {
#ifdef UNPACKED_MPQS
		LargeSpellIcons = LoadClx("ctrlpan\\spelicon_fg.clx");
		LargeSpellIconsBackground = LoadClx("ctrlpan\\spelicon_bg.clx");
#else
		LargeSpellIcons = LoadCel("ctrlpan\\spelicon", SPLICONLENGTH);
#endif
	} else {
#ifdef UNPACKED_MPQS
		LargeSpellIcons = LoadClx("data\\spelicon_fg.clx");
		LargeSpellIconsBackground = LoadClx("data\\spelicon_bg.clx");
#else
		LargeSpellIcons = LoadCel("data\\spelicon", SPLICONLENGTH);
#endif
	}
	SetSpellTrans(SpellType::Skill);
}

void FreeLargeSpellIcons()
{
#ifdef UNPACKED_MPQS
	LargeSpellIconsBackground = std::nullopt;
#endif
	LargeSpellIcons = std::nullopt;
}

void LoadSmallSpellIcons()
{
#ifdef UNPACKED_MPQS
	SmallSpellIcons = LoadClx("data\\spelli2_fg.clx");
	SmallSpellIconsBackground = LoadClx("data\\spelli2_bg.clx");
#else
	SmallSpellIcons = LoadCel("data\\spelli2", 37);
#endif
}

void FreeSmallSpellIcons()
{
#ifdef UNPACKED_MPQS
	SmallSpellIconsBackground = std::nullopt;
#endif
	SmallSpellIcons = std::nullopt;
}

void DrawLargeSpellIcon(const Surface &out, Point position, SpellID spell)
{
#ifdef UNPACKED_MPQS
	ClxDrawTRN(out, position, (*LargeSpellIconsBackground)[0], SplTransTbl);
#endif
	ClxDrawTRN(out, position, (*LargeSpellIcons)[SpellITbl[static_cast<int8_t>(spell)]], SplTransTbl);
}

void DrawSmallSpellIcon(const Surface &out, Point position, SpellID spell)
{
#ifdef UNPACKED_MPQS
	ClxDrawTRN(out, position, (*SmallSpellIconsBackground)[0], SplTransTbl);
#endif
	ClxDrawTRN(out, position, (*SmallSpellIcons)[SpellITbl[static_cast<int8_t>(spell)]], SplTransTbl);
}

void DrawLargeSpellIconBorder(const Surface &out, Point position, uint8_t color)
{
	const int width = (*LargeSpellIcons)[0].width();
	const int height = (*LargeSpellIcons)[0].height();
	UnsafeDrawBorder2px(out, Rectangle { Point { position.x, position.y - height + 1 }, Size { width, height } }, color);
}

void DrawSmallSpellIconBorder(const Surface &out, Point position)
{
	const int width = (*SmallSpellIcons)[0].width();
	const int height = (*SmallSpellIcons)[0].height();
	UnsafeDrawBorder2px(out, Rectangle { Point { position.x, position.y - height + 1 }, Size { width, height } }, SplTransTbl[PAL8_YELLOW + 2]);
}

void SetSpellTrans(SpellType t)
{
	if (t == SpellType::Skill) {
		for (int i = 0; i < 128; i++)
			SplTransTbl[i] = i;
	}
	for (int i = 128; i < 256; i++)
		SplTransTbl[i] = i;
	SplTransTbl[255] = 0;

	switch (t) {
	case SpellType::Spell:
		SplTransTbl[PAL8_YELLOW] = PAL16_BLUE + 1;
		SplTransTbl[PAL8_YELLOW + 1] = PAL16_BLUE + 3;
		SplTransTbl[PAL8_YELLOW + 2] = PAL16_BLUE + 5;
		for (int i = PAL16_BLUE; i < PAL16_BLUE + 16; i++) {
			SplTransTbl[PAL16_BEIGE - PAL16_BLUE + i] = i;
			SplTransTbl[PAL16_YELLOW - PAL16_BLUE + i] = i;
			SplTransTbl[PAL16_ORANGE - PAL16_BLUE + i] = i;
		}
		break;
	case SpellType::Scroll:
		SplTransTbl[PAL8_YELLOW] = PAL16_BEIGE + 1;
		SplTransTbl[PAL8_YELLOW + 1] = PAL16_BEIGE + 3;
		SplTransTbl[PAL8_YELLOW + 2] = PAL16_BEIGE + 5;
		for (int i = PAL16_BEIGE; i < PAL16_BEIGE + 16; i++) {
			SplTransTbl[PAL16_YELLOW - PAL16_BEIGE + i] = i;
			SplTransTbl[PAL16_ORANGE - PAL16_BEIGE + i] = i;
		}
		break;
	case SpellType::Charges:
		SplTransTbl[PAL8_YELLOW] = PAL16_ORANGE + 1;
		SplTransTbl[PAL8_YELLOW + 1] = PAL16_ORANGE + 3;
		SplTransTbl[PAL8_YELLOW + 2] = PAL16_ORANGE + 5;
		for (int i = PAL16_ORANGE; i < PAL16_ORANGE + 16; i++) {
			SplTransTbl[PAL16_BEIGE - PAL16_ORANGE + i] = i;
			SplTransTbl[PAL16_YELLOW - PAL16_ORANGE + i] = i;
		}
		break;
	case SpellType::Invalid:
		SplTransTbl[PAL8_YELLOW] = PAL16_GRAY + 1;
		SplTransTbl[PAL8_YELLOW + 1] = PAL16_GRAY + 3;
		SplTransTbl[PAL8_YELLOW + 2] = PAL16_GRAY + 5;
		for (int i = PAL16_GRAY; i < PAL16_GRAY + 15; i++) {
			SplTransTbl[PAL16_BEIGE - PAL16_GRAY + i] = i;
			SplTransTbl[PAL16_YELLOW - PAL16_GRAY + i] = i;
			SplTransTbl[PAL16_ORANGE - PAL16_GRAY + i] = i;
		}
		SplTransTbl[PAL16_BEIGE + 15] = 0;
		SplTransTbl[PAL16_YELLOW + 15] = 0;
		SplTransTbl[PAL16_ORANGE + 15] = 0;
		break;
	case SpellType::Skill:
		break;
	}
}

} // namespace devilution
