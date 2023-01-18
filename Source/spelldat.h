/**
 * @file spelldat.h
 *
 * Interface of all spell data.
 */
#pragma once

#include <cstdint>

#include "effects.h"

namespace devilution {

#define MAX_SPELLS 52

enum class SpellType : uint8_t {
	Skill,
	FIRST = Skill,
	Spell,
	Scroll,
	Charges,
	Invalid,
	LAST = Invalid,
};

enum spell_id : int8_t {
	SPL_NULL,
	SPL_FIREBOLT,
	SPL_HEAL,
	SPL_LIGHTNING,
	SPL_FLASH,
	SPL_IDENTIFY,
	SPL_FIREWALL,
	SPL_TOWN,
	SPL_STONE,
	SPL_INFRA,
	SPL_RNDTELEPORT,
	SPL_MANASHIELD,
	SPL_FIREBALL,
	SPL_GUARDIAN,
	SPL_CHAIN,
	SPL_WAVE,
	SPL_DOOMSERP,
	SPL_BLODRIT,
	SPL_NOVA,
	SPL_INVISIBIL,
	SPL_FLAME,
	SPL_GOLEM,
	SPL_BLODBOIL,
	SPL_TELEPORT,
	SPL_APOCA,
	SPL_ETHEREALIZE,
	SPL_REPAIR,
	SPL_RECHARGE,
	SPL_DISARM,
	SPL_ELEMENT,
	SPL_CBOLT,
	SPL_HBOLT,
	SPL_RESURRECT,
	SPL_TELEKINESIS,
	SPL_HEALOTHER,
	SPL_FLARE,
	SPL_BONESPIRIT,
	SPL_LASTDIABLO = SPL_BONESPIRIT,
	SPL_MANA,
	SPL_MAGI,
	SPL_JESTER,
	SPL_LIGHTWALL,
	SPL_IMMOLAT,
	SPL_WARP,
	SPL_REFLECT,
	SPL_BERSERK,
	SPL_FIRERING,
	SPL_SEARCH,
	SPL_RUNEFIRE,
	SPL_RUNELIGHT,
	SPL_RUNENOVA,
	SPL_RUNEIMMOLAT,
	SPL_RUNESTONE,

	SPL_LAST = SPL_RUNESTONE,
	SPL_INVALID = -1,
};

enum magic_type : uint8_t {
	STYPE_FIRE,
	STYPE_LIGHTNING,
	STYPE_MAGIC,
};

enum missile_id : int8_t {
	MIS_ARROW,
	MIS_FIREBOLT,
	MIS_GUARDIAN,
	MIS_RNDTELEPORT,
	MIS_LIGHTBALL,
	MIS_FIREWALL,
	MIS_FIREBALL,
	MIS_LIGHTCTRL,
	MIS_LIGHTNING,
	MIS_MISEXP,
	MIS_TOWN,
	MIS_FLASH,
	MIS_FLASH2,
	MIS_MANASHIELD,
	MIS_FIREMOVE,
	MIS_CHAIN,
	MIS_SENTINAL,
	MIS_BLODSTAR,
	MIS_BONE,
	MIS_METLHIT,
	MIS_RHINO,
	MIS_MAGMABALL,
	MIS_LIGHTCTRL2,
	MIS_LIGHTNING2,
	MIS_FLARE,
	MIS_MISEXP2,
	MIS_TELEPORT,
	MIS_FARROW,
	MIS_DOOMSERP,
	MIS_FIREWALLA,
	MIS_STONE,
	MIS_NULL_1F,
	MIS_INVISIBL,
	MIS_GOLEM,
	MIS_ETHEREALIZE,
	MIS_BLODBUR,
	MIS_BOOM,
	MIS_HEAL,
	MIS_FIREWALLC,
	MIS_INFRA,
	MIS_IDENTIFY,
	MIS_WAVE,
	MIS_NOVA,
	MIS_BLODBOIL,
	MIS_APOCA,
	MIS_REPAIR,
	MIS_RECHARGE,
	MIS_DISARM,
	MIS_FLAME,
	MIS_FLAMEC,
	MIS_FIREMAN,
	MIS_KRULL,
	MIS_CBOLT,
	MIS_HBOLT,
	MIS_RESURRECT,
	MIS_TELEKINESIS,
	MIS_LARROW,
	MIS_ACID,
	MIS_MISEXP3,
	MIS_ACIDPUD,
	MIS_HEALOTHER,
	MIS_ELEMENT,
	MIS_RESURRECTBEAM,
	MIS_BONESPIRIT,
	MIS_WEAPEXP,
	MIS_RPORTAL,
	MIS_BOOM2,
	MIS_DIABAPOCA,
	MIS_MANA,
	MIS_MAGI,
	MIS_LIGHTWALL,
	MIS_LIGHTNINGWALL,
	MIS_IMMOLATION,
	MIS_SPECARROW,
	MIS_FIRENOVA,
	MIS_LIGHTARROW,
	MIS_CBOLTARROW,
	MIS_HBOLTARROW,
	MIS_WARP,
	MIS_REFLECT,
	MIS_BERSERK,
	MIS_FIRERING,
	MIS_STEALPOTS,
	MIS_MANATRAP,
	MIS_LIGHTRING,
	MIS_SEARCH,
	MIS_FLASHFR,
	MIS_FLASHBK,
	MIS_IMMOLATION2,
	MIS_RUNEFIRE,
	MIS_RUNELIGHT,
	MIS_RUNENOVA,
	MIS_RUNEIMMOLAT,
	MIS_RUNESTONE,
	MIS_HIVEEXP,
	MIS_HORKDMN,
	MIS_JESTER,
	MIS_HIVEEXP2,
	MIS_LICH,
	MIS_PSYCHORB,
	MIS_NECROMORB,
	MIS_ARCHLICH,
	MIS_BONEDEMON,
	MIS_EXYEL2,
	MIS_EXRED3,
	MIS_EXBL2,
	MIS_EXBL3,
	MIS_EXORA1,
	MIS_NULL = -1,
};

struct SpellData {
	spell_id sName;
	uint8_t sManaCost;
	magic_type sType;
	const char *sNameText;
	int sBookLvl;
	int sStaffLvl;
	bool sTargeted;
	bool sTownSpell;
	int sMinInt;
	_sfx_id sSFX;
	missile_id sMissiles[3];
	uint8_t sManaAdj;
	uint8_t sMinMana;
	int sStaffMin;
	int sStaffMax;
	int sBookCost;
	int sStaffCost;
};

extern const SpellData spelldata[];

} // namespace devilution
