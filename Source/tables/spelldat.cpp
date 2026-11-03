/**
 * @file spelldat.cpp
 *
 * Implementation of all spell data.
 */
#include "tables/spelldat.h"

#include <algorithm>
#include <expected>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "appfat.h"
#include "data/file.hpp"
#include "data/iterators.hpp"
#include "data/record_reader.hpp"
#include "lua/lua_event.hpp"
#include "panels/spell_icons.hpp"
#include "utils/format.hpp"
#include "utils/language.h"

namespace devilution {

namespace {

void AddNullSpell()
{
	SpellData &null = SpellsData.emplace_back();
	null.sSFX = SfxID::None;
	null.bookCost10 = null.staffCost10 = null.sManaCost = 0;
	null.flags = SpellDataFlags::Fire;
	null.sBookLvl = null.sStaffLvl = 0;
	null.minInt = 0;
	null.sMissiles[0] = null.sMissiles[1] = MissileID::Null;
	null.sManaAdj = null.sMinMana = 0;
	null.sStaffMin = 40;
	null.sStaffMax = 80;
}

// A temporary solution for parsing soundID until we have a more general one.
std::expected<SfxID, std::string> ParseSpellSoundId(std::string_view value)
{
	if (value == "CastFire") return SfxID::CastFire;
	if (value == "CastHealing") return SfxID::CastHealing;
	if (value == "CastLightning") return SfxID::CastLightning;
	if (value == "CastSkill") return SfxID::CastSkill;
	return std::unexpected("Unknown enum value (only a few are supported for now)");
}

std::expected<SpellDataFlags, std::string> ParseSpellDataFlag(std::string_view value)
{
	if (value == "Fire") return SpellDataFlags::Fire;
	if (value == "Lightning") return SpellDataFlags::Lightning;
	if (value == "Magic") return SpellDataFlags::Magic;
	if (value == "Targeted") return SpellDataFlags::Targeted;
	if (value == "AllowedInTown") return SpellDataFlags::AllowedInTown;
	return std::unexpected("Unknown enum value");
}

std::expected<MissileID, std::string> ParseMissileId(std::string_view value)
{
	if (value == "Arrow") return MissileID::Arrow;
	if (value == "Firebolt") return MissileID::Firebolt;
	if (value == "Guardian") return MissileID::Guardian;
	if (value == "Phasing") return MissileID::Phasing;
	if (value == "NovaBall") return MissileID::NovaBall;
	if (value == "FireWall") return MissileID::FireWall;
	if (value == "Fireball") return MissileID::Fireball;
	if (value == "LightningControl") return MissileID::LightningControl;
	if (value == "Lightning") return MissileID::Lightning;
	if (value == "MagmaBallExplosion") return MissileID::MagmaBallExplosion;
	if (value == "TownPortal") return MissileID::TownPortal;
	if (value == "FlashBottom") return MissileID::FlashBottom;
	if (value == "FlashTop") return MissileID::FlashTop;
	if (value == "ManaShield") return MissileID::ManaShield;
	if (value == "FlameWave") return MissileID::FlameWave;
	if (value == "ChainLightning") return MissileID::ChainLightning;
	if (value == "ChainBall") return MissileID::ChainBall;
	if (value == "BloodHit") return MissileID::BloodHit;
	if (value == "BoneHit") return MissileID::BoneHit;
	if (value == "MetalHit") return MissileID::MetalHit;
	if (value == "Rhino") return MissileID::Rhino;
	if (value == "MagmaBall") return MissileID::MagmaBall;
	if (value == "ThinLightningControl") return MissileID::ThinLightningControl;
	if (value == "ThinLightning") return MissileID::ThinLightning;
	if (value == "BloodStar") return MissileID::BloodStar;
	if (value == "BloodStarExplosion") return MissileID::BloodStarExplosion;
	if (value == "Teleport") return MissileID::Teleport;
	if (value == "FireArrow") return MissileID::FireArrow;
	if (value == "DoomSerpents") return MissileID::DoomSerpents;
	if (value == "FireOnly") return MissileID::FireOnly;
	if (value == "StoneCurse") return MissileID::StoneCurse;
	if (value == "BloodRitual") return MissileID::BloodRitual;
	if (value == "Invisibility") return MissileID::Invisibility;
	if (value == "Golem") return MissileID::Golem;
	if (value == "Etherealize") return MissileID::Etherealize;
	if (value == "Spurt") return MissileID::Spurt;
	if (value == "ApocalypseBoom") return MissileID::ApocalypseBoom;
	if (value == "Healing") return MissileID::Healing;
	if (value == "FireWallControl") return MissileID::FireWallControl;
	if (value == "Infravision") return MissileID::Infravision;
	if (value == "Identify") return MissileID::Identify;
	if (value == "FlameWaveControl") return MissileID::FlameWaveControl;
	if (value == "Nova") return MissileID::Nova;
	if (value == "Rage") return MissileID::Rage;
	if (value == "Apocalypse") return MissileID::Apocalypse;
	if (value == "ItemRepair") return MissileID::ItemRepair;
	if (value == "StaffRecharge") return MissileID::StaffRecharge;
	if (value == "TrapDisarm") return MissileID::TrapDisarm;
	if (value == "Inferno") return MissileID::Inferno;
	if (value == "InfernoControl") return MissileID::InfernoControl;
	if (value == "FireMan") return MissileID::FireMan;
	if (value == "Krull") return MissileID::Krull;
	if (value == "ChargedBolt") return MissileID::ChargedBolt;
	if (value == "HolyBolt") return MissileID::HolyBolt;
	if (value == "Resurrect") return MissileID::Resurrect;
	if (value == "Telekinesis") return MissileID::Telekinesis;
	if (value == "LightningArrow") return MissileID::LightningArrow;
	if (value == "Acid") return MissileID::Acid;
	if (value == "AcidSplat") return MissileID::AcidSplat;
	if (value == "AcidPuddle") return MissileID::AcidPuddle;
	if (value == "HealOther") return MissileID::HealOther;
	if (value == "Elemental") return MissileID::Elemental;
	if (value == "ResurrectBeam") return MissileID::ResurrectBeam;
	if (value == "BoneSpirit") return MissileID::BoneSpirit;
	if (value == "WeaponExplosion") return MissileID::WeaponExplosion;
	if (value == "RedPortal") return MissileID::RedPortal;
	if (value == "DiabloApocalypseBoom") return MissileID::DiabloApocalypseBoom;
	if (value == "DiabloApocalypse") return MissileID::DiabloApocalypse;
	if (value == "Mana") return MissileID::Mana;
	if (value == "Magi") return MissileID::Magi;
	if (value == "LightningWall") return MissileID::LightningWall;
	if (value == "LightningWallControl") return MissileID::LightningWallControl;
	if (value == "Immolation") return MissileID::Immolation;
	if (value == "SpectralArrow") return MissileID::SpectralArrow;
	if (value == "FireballBow") return MissileID::FireballBow;
	if (value == "LightningBow") return MissileID::LightningBow;
	if (value == "ChargedBoltBow") return MissileID::ChargedBoltBow;
	if (value == "HolyBoltBow") return MissileID::HolyBoltBow;
	if (value == "Warp") return MissileID::Warp;
	if (value == "Reflect") return MissileID::Reflect;
	if (value == "Berserk") return MissileID::Berserk;
	if (value == "RingOfFire") return MissileID::RingOfFire;
	if (value == "StealPotions") return MissileID::StealPotions;
	if (value == "StealMana") return MissileID::StealMana;
	if (value == "RingOfLightning") return MissileID::RingOfLightning;
	if (value == "Search") return MissileID::Search;
	if (value == "Aura") return MissileID::Aura;
	if (value == "Aura2") return MissileID::Aura2;
	if (value == "SpiralFireball") return MissileID::SpiralFireball;
	if (value == "RuneOfFire") return MissileID::RuneOfFire;
	if (value == "RuneOfLight") return MissileID::RuneOfLight;
	if (value == "RuneOfNova") return MissileID::RuneOfNova;
	if (value == "RuneOfImmolation") return MissileID::RuneOfImmolation;
	if (value == "RuneOfStone") return MissileID::RuneOfStone;
	if (value == "BigExplosion") return MissileID::BigExplosion;
	if (value == "HorkSpawn") return MissileID::HorkSpawn;
	if (value == "Jester") return MissileID::Jester;
	if (value == "OpenNest") return MissileID::OpenNest;
	if (value == "OrangeFlare") return MissileID::OrangeFlare;
	if (value == "BlueFlare") return MissileID::BlueFlare;
	if (value == "RedFlare") return MissileID::RedFlare;
	if (value == "YellowFlare") return MissileID::YellowFlare;
	if (value == "BlueFlare2") return MissileID::BlueFlare2;
	if (value == "YellowExplosion") return MissileID::YellowExplosion;
	if (value == "RedExplosion") return MissileID::RedExplosion;
	if (value == "BlueExplosion") return MissileID::BlueExplosion;
	if (value == "BlueExplosion2") return MissileID::BlueExplosion2;
	if (value == "OrangeExplosion") return MissileID::OrangeExplosion;
	return std::unexpected("Unknown enum value");
}

} // namespace

/** Data related to each spell ID. */
std::vector<SpellData> SpellsData;

namespace {
std::unordered_map<std::string, SpellID> LuaDynamicSpellIds; // Lua mod support: mod spell ids, resolved by ParseSpellId
} // namespace

std::expected<SpellID, std::string> ParseSpellId(std::string_view value)
{
	if (value == "Null") return SpellID::Null;
	if (value == "Firebolt") return SpellID::Firebolt;
	if (value == "Healing") return SpellID::Healing;
	if (value == "Lightning") return SpellID::Lightning;
	if (value == "Flash") return SpellID::Flash;
	if (value == "Identify") return SpellID::Identify;
	if (value == "FireWall") return SpellID::FireWall;
	if (value == "TownPortal") return SpellID::TownPortal;
	if (value == "StoneCurse") return SpellID::StoneCurse;
	if (value == "Infravision") return SpellID::Infravision;
	if (value == "Phasing") return SpellID::Phasing;
	if (value == "ManaShield") return SpellID::ManaShield;
	if (value == "Fireball") return SpellID::Fireball;
	if (value == "Guardian") return SpellID::Guardian;
	if (value == "ChainLightning") return SpellID::ChainLightning;
	if (value == "FlameWave") return SpellID::FlameWave;
	if (value == "DoomSerpents") return SpellID::DoomSerpents;
	if (value == "BloodRitual") return SpellID::BloodRitual;
	if (value == "Nova") return SpellID::Nova;
	if (value == "Invisibility") return SpellID::Invisibility;
	if (value == "Inferno") return SpellID::Inferno;
	if (value == "Golem") return SpellID::Golem;
	if (value == "Rage") return SpellID::Rage;
	if (value == "Teleport") return SpellID::Teleport;
	if (value == "Apocalypse") return SpellID::Apocalypse;
	if (value == "Etherealize") return SpellID::Etherealize;
	if (value == "ItemRepair") return SpellID::ItemRepair;
	if (value == "StaffRecharge") return SpellID::StaffRecharge;
	if (value == "TrapDisarm") return SpellID::TrapDisarm;
	if (value == "Elemental") return SpellID::Elemental;
	if (value == "ChargedBolt") return SpellID::ChargedBolt;
	if (value == "HolyBolt") return SpellID::HolyBolt;
	if (value == "Resurrect") return SpellID::Resurrect;
	if (value == "Telekinesis") return SpellID::Telekinesis;
	if (value == "HealOther") return SpellID::HealOther;
	if (value == "BloodStar") return SpellID::BloodStar;
	if (value == "BoneSpirit") return SpellID::BoneSpirit;
	if (value == "Mana") return SpellID::Mana;
	if (value == "Magi") return SpellID::Magi;
	if (value == "Jester") return SpellID::Jester;
	if (value == "LightningWall") return SpellID::LightningWall;
	if (value == "Immolation") return SpellID::Immolation;
	if (value == "Warp") return SpellID::Warp;
	if (value == "Reflect") return SpellID::Reflect;
	if (value == "Berserk") return SpellID::Berserk;
	if (value == "RingOfFire") return SpellID::RingOfFire;
	if (value == "Search") return SpellID::Search;
	if (value == "RuneOfFire") return SpellID::RuneOfFire;
	if (value == "RuneOfLight") return SpellID::RuneOfLight;
	if (value == "RuneOfNova") return SpellID::RuneOfNova;
	if (value == "RuneOfImmolation") return SpellID::RuneOfImmolation;
	if (value == "RuneOfStone") return SpellID::RuneOfStone;
	const auto it = LuaDynamicSpellIds.find(std::string(value)); // Lua mod support: dynamic spell names registered by mods
	if (it != LuaDynamicSpellIds.end()) return it->second;
	return std::unexpected("Unknown enum value");
}

namespace {

// Reads the fields after `id` of one spell record into `item`. Shared by the base-table loader and
// mod spell data so both parse identically. // Lua mod support
void ReadSpellRecord(RecordReader &reader, SpellData &item)
{
	reader.readString("name", item.sNameText);
	reader.read("soundId", item.sSFX, ParseSpellSoundId);
	reader.readInt("bookCost10", item.bookCost10);
	reader.readInt("staffCost10", item.staffCost10);
	reader.readInt("manaCost", item.sManaCost);
	reader.readEnumList("flags", item.flags, ParseSpellDataFlag);
	reader.readInt("bookLevel", item.sBookLvl);
	reader.readInt("staffLevel", item.sStaffLvl);
	reader.readInt("minIntelligence", item.minInt);
	reader.readEnumArray("missiles", /*fillMissing=*/std::make_optional(MissileID::Null), item.sMissiles, ParseMissileId);
	reader.readInt("manaMultiplier", item.sManaAdj);
	reader.readInt("minMana", item.sMinMana);
	reader.readInt("staffMin", item.sStaffMin);
	reader.readInt("staffMax", item.sStaffMax);
}

// A non-learnable placeholder used to pad gaps when a dynamic spell is assigned an ID beyond the
// loaded base table (the base table is shorter in Diablo than in Hellfire). bookLevel/staffLevel = -1
// keep GetSpellBookLevel/GetSpellStaffLevel returning -1 for the gap exactly as the engine's
// out-of-bounds path does, so the save layout is unchanged. // Lua mod support
void MakeInertSpell(SpellData &s)
{
	s.sNameText.clear();
	s.sSFX = SfxID::None;
	s.bookCost10 = s.staffCost10 = s.sManaCost = 0;
	s.flags = SpellDataFlags::Fire;
	s.sBookLvl = s.sStaffLvl = -1;
	s.minInt = 0;
	s.sMissiles[0] = s.sMissiles[1] = MissileID::Null;
	s.sManaAdj = s.sMinMana = 0;
	s.sStaffMin = 40;
	s.sStaffMax = 80;
}

// Lua mod support: spell records added by mods during SpellDataLoaded, assigned IDs in LuaFinalizeDynamicSpells.
struct PendingDynamicSpell {
	std::string id;
	SpellData data;
};
std::vector<PendingDynamicSpell> PendingDynamicSpells;

// The per-player spell sets are 64-bit (GetSpellBitmask shifts by id-1), so spell IDs must stay below 64.
constexpr int MaxSpellId = 63;

void LoadSpellDatFromFile(DataFile &dataFile, std::string_view filename)
{
	dataFile.skipHeaderOrDie(filename);
	SpellsData.reserve(SpellsData.size() + dataFile.numRecords());
	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		reader.advance(); // skip id
		ReadSpellRecord(reader, SpellsData.emplace_back());
	}
}

void LuaClearPendingDynamicSpells()
{
	PendingDynamicSpells.clear();
}

void LuaFinalizeDynamicSpells()
{
	if (PendingDynamicSpells.empty())
		return;
	// Deterministic, mode-independent IDs: sort the added spells by id and assign from a fixed base
	// (just past the static SpellID range) so a given spell id resolves to the same SpellID in every game
	// mode and regardless of mod load order. Every client running the same mod set derives the same map.
	std::sort(PendingDynamicSpells.begin(), PendingDynamicSpells.end(),
	    [](const PendingDynamicSpell &a, const PendingDynamicSpell &b) { return a.id < b.id; });
	const int firstId = static_cast<int>(SpellID::LAST) + 1;
	if (firstId + static_cast<int>(PendingDynamicSpells.size()) - 1 > MaxSpellId) {
		DisplayFatalErrorAndExit(_("Loading Spell Data Failed"), FormatRuntime(_("Could not add {} spells, since the maximum spell id of {} would be exceeded."), PendingDynamicSpells.size(), MaxSpellId));
	}
	int nextId = firstId;
	for (PendingDynamicSpell &pending : PendingDynamicSpells) {
		if (ParseSpellId(pending.id).has_value()) {
			DisplayFatalErrorAndExit(_("Loading Spell Data Failed"), FormatRuntime(_("Spell id {} is already in use."), pending.id));
		}
		// Pad up to nextId with inert placeholders so direct indexing stays valid even when the loaded
		// base table is shorter than the dynamic ID region (e.g. Diablo vs Hellfire).
		while (static_cast<int>(SpellsData.size()) <= nextId)
			MakeInertSpell(SpellsData.emplace_back());
		SpellsData[nextId] = std::move(pending.data);
		LuaDynamicSpellIds.emplace(pending.id, static_cast<SpellID>(nextId));
		++nextId;
	}
	PendingDynamicSpells.clear();
}

} // namespace

void LuaAddSpellDatFromFile(DataFile &dataFile, std::string_view filename)
{
	dataFile.skipHeaderOrDie(filename);
	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		PendingDynamicSpell &pending = PendingDynamicSpells.emplace_back();
		reader.readString("id", pending.id);
		ReadSpellRecord(reader, pending.data);
	}
}

void LoadSpellData()
{
	LuaDynamicSpellIds.clear();
	LuaClearDynamicSpellIcons();
	LuaClearPendingDynamicSpells();
	SpellsData.clear();
	const std::string_view filename = "txtdata\\spells\\spelldat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	SpellsData.reserve(dataFile.numRecords() + 1);
	AddNullSpell();
	LoadSpellDatFromFile(dataFile, filename);
	lua::SpellDataLoaded();
	LuaFinalizeDynamicSpells();
	lua::SpellsAssigned();
	SpellsData.shrink_to_fit();
}

} // namespace devilution
