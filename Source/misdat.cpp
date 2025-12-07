/**
 * @file misdat.cpp
 *
 * Implementation of data related to missiles.
 */
#include "misdat.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include <expected.hpp>

#include "data/file.hpp"
#include "data/iterators.hpp"
#include "data/record_reader.hpp"
#include "engine/clx_sprite.hpp"
#include "headless_mode.hpp"
#include "missiles.h"
#include "mpq/mpq_common.hpp"
#include "sound_effect_enums.h"
#include "spelldat.h"
#include "utils/file_name_generator.hpp"
#include "utils/status_macros.hpp"
#include "utils/str_cat.hpp"

#ifdef UNPACKED_MPQS
#include "engine/load_clx.hpp"
#else
#include "engine/load_cl2.hpp"
#endif

namespace devilution {

namespace {

/** Data related to each missile graphic ID. */
std::vector<MissileFileData> MissileSpriteData;
std::vector<std::array<uint8_t, 16>> MissileAnimDelays;
std::vector<std::array<uint8_t, 16>> MissileAnimLengths;

/** Data related to each missile ID. */
std::vector<MissileData> MissilesData;

size_t ToIndex(std::vector<std::array<uint8_t, 16>> &all, const std::array<uint8_t, 16> &value)
{
	for (size_t i = 0; i < all.size(); ++i) {
		if (all[i] == value) return i;
	}
	all.push_back(value);
	return all.size() - 1;
}

tl::expected<MissileGraphicsFlags, std::string> ParseMissileGraphicsFlag(std::string_view value)
{
	if (value.empty()) return MissileGraphicsFlags::None;
	if (value == "MonsterOwned") return MissileGraphicsFlags::MonsterOwned;
	if (value == "NotAnimated") return MissileGraphicsFlags::NotAnimated;
	return tl::make_unexpected("Unknown enum value");
}

tl::expected<MissileGraphicID, std::string> ParseMissileGraphicID(std::string_view value)
{
	if (value.empty()) return MissileGraphicID::None;
	if (value == "Arrow") return MissileGraphicID::Arrow;
	if (value == "Fireball") return MissileGraphicID::Fireball;
	if (value == "Guardian") return MissileGraphicID::Guardian;
	if (value == "Lightning") return MissileGraphicID::Lightning;
	if (value == "FireWall") return MissileGraphicID::FireWall;
	if (value == "MagmaBallExplosion") return MissileGraphicID::MagmaBallExplosion;
	if (value == "TownPortal") return MissileGraphicID::TownPortal;
	if (value == "FlashBottom") return MissileGraphicID::FlashBottom;
	if (value == "FlashTop") return MissileGraphicID::FlashTop;
	if (value == "ManaShield") return MissileGraphicID::ManaShield;
	if (value == "BloodHit") return MissileGraphicID::BloodHit;
	if (value == "BoneHit") return MissileGraphicID::BoneHit;
	if (value == "MetalHit") return MissileGraphicID::MetalHit;
	if (value == "FireArrow") return MissileGraphicID::FireArrow;
	if (value == "DoomSerpents") return MissileGraphicID::DoomSerpents;
	if (value == "Golem") return MissileGraphicID::Golem;
	if (value == "Spurt") return MissileGraphicID::Spurt;
	if (value == "ApocalypseBoom") return MissileGraphicID::ApocalypseBoom;
	if (value == "StoneCurseShatter") return MissileGraphicID::StoneCurseShatter;
	if (value == "BigExplosion") return MissileGraphicID::BigExplosion;
	if (value == "Inferno") return MissileGraphicID::Inferno;
	if (value == "ThinLightning") return MissileGraphicID::ThinLightning;
	if (value == "BloodStar") return MissileGraphicID::BloodStar;
	if (value == "BloodStarExplosion") return MissileGraphicID::BloodStarExplosion;
	if (value == "MagmaBall") return MissileGraphicID::MagmaBall;
	if (value == "Krull") return MissileGraphicID::Krull;
	if (value == "ChargedBolt") return MissileGraphicID::ChargedBolt;
	if (value == "HolyBolt") return MissileGraphicID::HolyBolt;
	if (value == "HolyBoltExplosion") return MissileGraphicID::HolyBoltExplosion;
	if (value == "LightningArrow") return MissileGraphicID::LightningArrow;
	if (value == "FireArrowExplosion") return MissileGraphicID::FireArrowExplosion;
	if (value == "Acid") return MissileGraphicID::Acid;
	if (value == "AcidSplat") return MissileGraphicID::AcidSplat;
	if (value == "AcidPuddle") return MissileGraphicID::AcidPuddle;
	if (value == "Etherealize") return MissileGraphicID::Etherealize;
	if (value == "Elemental") return MissileGraphicID::Elemental;
	if (value == "Resurrect") return MissileGraphicID::Resurrect;
	if (value == "BoneSpirit") return MissileGraphicID::BoneSpirit;
	if (value == "RedPortal") return MissileGraphicID::RedPortal;
	if (value == "DiabloApocalypseBoom") return MissileGraphicID::DiabloApocalypseBoom;
	if (value == "BloodStarBlue") return MissileGraphicID::BloodStarBlue;
	if (value == "BloodStarBlueExplosion") return MissileGraphicID::BloodStarBlueExplosion;
	if (value == "BloodStarYellow") return MissileGraphicID::BloodStarYellow;
	if (value == "BloodStarYellowExplosion") return MissileGraphicID::BloodStarYellowExplosion;
	if (value == "BloodStarRed") return MissileGraphicID::BloodStarRed;
	if (value == "BloodStarRedExplosion") return MissileGraphicID::BloodStarRedExplosion;
	if (value == "HorkSpawn") return MissileGraphicID::HorkSpawn;
	if (value == "Reflect") return MissileGraphicID::Reflect;
	if (value == "OrangeFlare") return MissileGraphicID::OrangeFlare;
	if (value == "BlueFlare") return MissileGraphicID::BlueFlare;
	if (value == "RedFlare") return MissileGraphicID::RedFlare;
	if (value == "YellowFlare") return MissileGraphicID::YellowFlare;
	if (value == "Rune") return MissileGraphicID::Rune;
	if (value == "YellowFlareExplosion") return MissileGraphicID::YellowFlareExplosion;
	if (value == "BlueFlareExplosion") return MissileGraphicID::BlueFlareExplosion;
	if (value == "RedFlareExplosion") return MissileGraphicID::RedFlareExplosion;
	if (value == "BlueFlare2") return MissileGraphicID::BlueFlare2;
	if (value == "OrangeFlareExplosion") return MissileGraphicID::OrangeFlareExplosion;
	if (value == "BlueFlareExplosion2") return MissileGraphicID::BlueFlareExplosion2;
	return tl::make_unexpected("Unknown enum value");
}

void LoadMissileSpriteData()
{
	const std::string_view filename = "txtdata\\missiles\\missile_sprites.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	dataFile.skipHeaderOrDie(filename);

	MissileAnimDelays.clear();
	MissileAnimLengths.clear();
	MissileSpriteData.clear();
	MissileSpriteData.reserve(dataFile.numRecords());

	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		MissileFileData &item = MissileSpriteData.emplace_back();
		MissileGraphicID id;
		reader.read("id", id, ParseMissileGraphicID);
		assert(static_cast<size_t>(id) + 1 == MissileSpriteData.size());
		reader.readInt("width", item.animWidth);
		reader.readInt("width2", item.animWidth2);
		reader.readString("name", item.name);
		reader.readInt("numFrames", item.animFAmt);
		reader.read("flags", item.flags, ParseMissileGraphicsFlag);

		std::array<uint8_t, 16> arr;
		reader.readIntArray("frameDelay", arr);
		item.animDelayIdx = static_cast<uint8_t>(ToIndex(MissileAnimDelays, arr));

		reader.readIntArray("frameLength", arr);
		item.animLenIdx = static_cast<uint8_t>(ToIndex(MissileAnimLengths, arr));
	}

	MissileSpriteData.shrink_to_fit();
	MissileAnimDelays.shrink_to_fit();
	MissileAnimLengths.shrink_to_fit();
}

tl::expected<MissileDataFlags, std::string> ParseMissileDataFlag(std::string_view value)
{
	if (value == "Physical") return MissileDataFlags::Physical;
	if (value == "Fire") return MissileDataFlags::Fire;
	if (value == "Lightning") return MissileDataFlags::Lightning;
	if (value == "Magic") return MissileDataFlags::Magic;
	if (value == "Acid") return MissileDataFlags::Acid;
	if (value == "Arrow") return MissileDataFlags::Arrow;
	if (value == "Invisible") return MissileDataFlags::Invisible;
	return tl::make_unexpected("Unknown enum value");
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
tl::expected<MissileData::AddFn, std::string> ParseMissileAddFn(std::string_view value)
{
	if (value.empty()) return nullptr;
	if (value == "AddOpenNest") return AddOpenNest;
	if (value == "AddRuneOfFire") return AddRuneOfFire;
	if (value == "AddRuneOfLight") return AddRuneOfLight;
	if (value == "AddRuneOfNova") return AddRuneOfNova;
	if (value == "AddRuneOfImmolation") return AddRuneOfImmolation;
	if (value == "AddRuneOfStone") return AddRuneOfStone;
	if (value == "AddReflect") return AddReflect;
	if (value == "AddBerserk") return AddBerserk;
	if (value == "AddHorkSpawn") return AddHorkSpawn;
	if (value == "AddJester") return AddJester;
	if (value == "AddStealPotions") return AddStealPotions;
	if (value == "AddStealMana") return AddStealMana;
	if (value == "AddSpectralArrow") return AddSpectralArrow;
	if (value == "AddWarp") return AddWarp;
	if (value == "AddLightningWall") return AddLightningWall;
	if (value == "AddBigExplosion") return AddBigExplosion;
	if (value == "AddImmolation") return AddImmolation;
	if (value == "AddLightningBow") return AddLightningBow;
	if (value == "AddMana") return AddMana;
	if (value == "AddMagi") return AddMagi;
	if (value == "AddRingOfFire") return AddRingOfFire;
	if (value == "AddSearch") return AddSearch;
	if (value == "AddChargedBoltBow") return AddChargedBoltBow;
	if (value == "AddElementalArrow") return AddElementalArrow;
	if (value == "AddArrow") return AddArrow;
	if (value == "AddPhasing") return AddPhasing;
	if (value == "AddFirebolt") return AddFirebolt;
	if (value == "AddMagmaBall") return AddMagmaBall;
	if (value == "AddTeleport") return AddTeleport;
	if (value == "AddNovaBall") return AddNovaBall;
	if (value == "AddFireWall") return AddFireWall;
	if (value == "AddFireball") return AddFireball;
	if (value == "AddLightningControl") return AddLightningControl;
	if (value == "AddLightning") return AddLightning;
	if (value == "AddMissileExplosion") return AddMissileExplosion;
	if (value == "AddWeaponExplosion") return AddWeaponExplosion;
	if (value == "AddTownPortal") return AddTownPortal;
	if (value == "AddFlashBottom") return AddFlashBottom;
	if (value == "AddFlashTop") return AddFlashTop;
	if (value == "AddManaShield") return AddManaShield;
	if (value == "AddFlameWave") return AddFlameWave;
	if (value == "AddGuardian") return AddGuardian;
	if (value == "AddChainLightning") return AddChainLightning;
	if (value == "AddRhino") return AddRhino;
	if (value == "AddGenericMagicMissile") return AddGenericMagicMissile;
	if (value == "AddAcid") return AddAcid;
	if (value == "AddAcidPuddle") return AddAcidPuddle;
	if (value == "AddStoneCurse") return AddStoneCurse;
	if (value == "AddGolem") return AddGolem;
	if (value == "AddApocalypseBoom") return AddApocalypseBoom;
	if (value == "AddHealing") return AddHealing;
	if (value == "AddHealOther") return AddHealOther;
	if (value == "AddElemental") return AddElemental;
	if (value == "AddIdentify") return AddIdentify;
	if (value == "AddWallControl") return AddWallControl;
	if (value == "AddInfravision") return AddInfravision;
	if (value == "AddFlameWaveControl") return AddFlameWaveControl;
	if (value == "AddNova") return AddNova;
	if (value == "AddRage") return AddRage;
	if (value == "AddItemRepair") return AddItemRepair;
	if (value == "AddStaffRecharge") return AddStaffRecharge;
	if (value == "AddTrapDisarm") return AddTrapDisarm;
	if (value == "AddApocalypse") return AddApocalypse;
	if (value == "AddInferno") return AddInferno;
	if (value == "AddInfernoControl") return AddInfernoControl;
	if (value == "AddChargedBolt") return AddChargedBolt;
	if (value == "AddHolyBolt") return AddHolyBolt;
	if (value == "AddResurrect") return AddResurrect;
	if (value == "AddResurrectBeam") return AddResurrectBeam;
	if (value == "AddTelekinesis") return AddTelekinesis;
	if (value == "AddBoneSpirit") return AddBoneSpirit;
	if (value == "AddRedPortal") return AddRedPortal;
	if (value == "AddDiabloApocalypse") return AddDiabloApocalypse;
	return tl::make_unexpected("Unknown MissileData::AddFn name");
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity)
tl::expected<MissileData::ProcessFn, std::string> ParseMissileProcessFn(std::string_view value)
{
	if (value.empty()) return nullptr;
	if (value == "ProcessElementalArrow") return ProcessElementalArrow;
	if (value == "ProcessArrow") return ProcessArrow;
	if (value == "ProcessGenericProjectile") return ProcessGenericProjectile;
	if (value == "ProcessNovaBall") return ProcessNovaBall;
	if (value == "ProcessAcidPuddle") return ProcessAcidPuddle;
	if (value == "ProcessFireWall") return ProcessFireWall;
	if (value == "ProcessFireball") return ProcessFireball;
	if (value == "ProcessHorkSpawn") return ProcessHorkSpawn;
	if (value == "ProcessRune") return ProcessRune;
	if (value == "ProcessLightningWall") return ProcessLightningWall;
	if (value == "ProcessBigExplosion") return ProcessBigExplosion;
	if (value == "ProcessLightningBow") return ProcessLightningBow;
	if (value == "ProcessRingOfFire") return ProcessRingOfFire;
	if (value == "ProcessSearch") return ProcessSearch;
	if (value == "ProcessImmolation") return ProcessImmolation;
	if (value == "ProcessSpectralArrow") return ProcessSpectralArrow;
	if (value == "ProcessLightningControl") return ProcessLightningControl;
	if (value == "ProcessLightning") return ProcessLightning;
	if (value == "ProcessTownPortal") return ProcessTownPortal;
	if (value == "ProcessFlashBottom") return ProcessFlashBottom;
	if (value == "ProcessFlashTop") return ProcessFlashTop;
	if (value == "ProcessFlameWave") return ProcessFlameWave;
	if (value == "ProcessGuardian") return ProcessGuardian;
	if (value == "ProcessChainLightning") return ProcessChainLightning;
	if (value == "ProcessWeaponExplosion") return ProcessWeaponExplosion;
	if (value == "ProcessMissileExplosion") return ProcessMissileExplosion;
	if (value == "ProcessAcidSplate") return ProcessAcidSplate;
	if (value == "ProcessTeleport") return ProcessTeleport;
	if (value == "ProcessStoneCurse") return ProcessStoneCurse;
	if (value == "ProcessApocalypseBoom") return ProcessApocalypseBoom;
	if (value == "ProcessRhino") return ProcessRhino;
	if (value == "ProcessWallControl") return ProcessWallControl;
	if (value == "ProcessInfravision") return ProcessInfravision;
	if (value == "ProcessApocalypse") return ProcessApocalypse;
	if (value == "ProcessFlameWaveControl") return ProcessFlameWaveControl;
	if (value == "ProcessNova") return ProcessNova;
	if (value == "ProcessRage") return ProcessRage;
	if (value == "ProcessInferno") return ProcessInferno;
	if (value == "ProcessInfernoControl") return ProcessInfernoControl;
	if (value == "ProcessChargedBolt") return ProcessChargedBolt;
	if (value == "ProcessHolyBolt") return ProcessHolyBolt;
	if (value == "ProcessElemental") return ProcessElemental;
	if (value == "ProcessBoneSpirit") return ProcessBoneSpirit;
	if (value == "ProcessResurrectBeam") return ProcessResurrectBeam;
	if (value == "ProcessRedPortal") return ProcessRedPortal;
	return tl::make_unexpected("Unknown MissileData::ProcessFn name");
}

// A temporary solution for parsing SfxID until we have a more general one.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
tl::expected<SfxID, std::string> ParseCastSound(std::string_view value)
{
	if (value.empty()) return SfxID::None;
	if (value == "BigExplosion") return SfxID::BigExplosion;
	if (value == "ShootFireballBow") return SfxID::ShootFireballBow;
	if (value == "SpellAcid") return SfxID::SpellAcid;
	if (value == "SpellApocalypse") return SfxID::SpellApocalypse;
	if (value == "SpellBloodStar") return SfxID::SpellBloodStar;
	if (value == "SpellBoneSpirit") return SfxID::SpellBoneSpirit;
	if (value == "SpellChargedBolt") return SfxID::SpellChargedBolt;
	if (value == "SpellDoomSerpents") return SfxID::SpellDoomSerpents;
	if (value == "SpellElemental") return SfxID::SpellElemental;
	if (value == "SpellEnd") return SfxID::SpellEnd;
	if (value == "SpellEtherealize") return SfxID::SpellEtherealize;
	if (value == "SpellFireHit") return SfxID::SpellFireHit;
	if (value == "SpellFireWall") return SfxID::SpellFireWall;
	if (value == "SpellFirebolt") return SfxID::SpellFirebolt;
	if (value == "SpellFlameWave") return SfxID::SpellFlameWave;
	if (value == "SpellGolem") return SfxID::SpellGolem;
	if (value == "SpellGuardian") return SfxID::SpellGuardian;
	if (value == "SpellHolyBolt") return SfxID::SpellHolyBolt;
	if (value == "SpellInferno") return SfxID::SpellInferno;
	if (value == "SpellInfravision") return SfxID::SpellInfravision;
	if (value == "SpellInvisibility") return SfxID::SpellInvisibility;
	if (value == "SpellLightning") return SfxID::SpellLightning;
	if (value == "SpellLightningWall") return SfxID::SpellLightningWall;
	if (value == "SpellManaShield") return SfxID::SpellManaShield;
	if (value == "SpellNova") return SfxID::SpellNova;
	if (value == "SpellPortal") return SfxID::SpellPortal;
	if (value == "SpellPuddle") return SfxID::SpellPuddle;
	if (value == "SpellStoneCurse") return SfxID::SpellStoneCurse;
	if (value == "SpellTeleport") return SfxID::SpellTeleport;
	if (value == "SpellTrapDisarm") return SfxID::SpellTrapDisarm;
	return tl::make_unexpected("Unknown enum value (only a few are supported for now)");
}

// A temporary solution for parsing SfxID until we have a more general one.
tl::expected<SfxID, std::string> ParseHitSound(std::string_view value)
{
	if (value.empty()) return SfxID::None;
	if (value == "BigExplosion") return SfxID::BigExplosion;
	if (value == "SpellBloodStarHit") return SfxID::SpellBloodStarHit;
	if (value == "SpellBoneSpiritHit") return SfxID::SpellBoneSpiritHit;
	if (value == "SpellFireHit") return SfxID::SpellFireHit;
	if (value == "SpellLightningHit") return SfxID::SpellLightningHit;
	if (value == "SpellResurrect") return SfxID::SpellResurrect;
	return tl::make_unexpected("Unknown enum value (only a few are supported for now)");
}

tl::expected<MissileMovementDistribution, std::string> ParseMissileMovementDistribution(std::string_view value)
{
	if (value.empty()) return MissileMovementDistribution::Disabled;
	if (value == "Blockable") return MissileMovementDistribution::Blockable;
	if (value == "Unblockable") return MissileMovementDistribution::Unblockable;
	return tl::make_unexpected("Unknown enum value");
}

void LoadMisdat()
{
	const std::string_view filename = "txtdata\\missiles\\misdat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);
	dataFile.skipHeaderOrDie(filename);

	MissilesData.clear();
	MissilesData.reserve(dataFile.numRecords());
	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };
		MissileData &item = MissilesData.emplace_back();
		reader.advance(); // skip id
		reader.read("addFn", item.addFn, ParseMissileAddFn);
		reader.read("processFn", item.processFn, ParseMissileProcessFn);
		reader.read("castSound", item.castSound, ParseCastSound);
		reader.read("hitSound", item.hitSound, ParseHitSound);
		reader.read("graphic", item.graphic, ParseMissileGraphicID);
		reader.readEnumList("flags", item.flags, ParseMissileDataFlag);
		reader.read("movementDistribution", item.movementDistribution, ParseMissileMovementDistribution);
	}

	// Sanity check because we do not actually parse the IDs yet.
	assert(static_cast<size_t>(MissileID::LastDiablo) + 1 == MissilesData.size() || static_cast<size_t>(MissileID::LAST) + 1 == MissilesData.size());

	MissilesData.shrink_to_fit();
}

} // namespace

uint8_t MissileFileData::animDelay(uint8_t dir) const
{
	return MissileAnimDelays[animDelayIdx][dir];
}

uint8_t MissileFileData::animLen(uint8_t dir) const
{
	return MissileAnimLengths[animLenIdx][dir];
}

tl::expected<void, std::string> MissileFileData::LoadGFX()
{
	if (sprites)
		return {};

	if (name[0] == '\0')
		return {};

#ifdef UNPACKED_MPQS
	char path[MaxMpqPathSize];
	*BufCopy(path, "missiles\\", name, ".clx") = '\0';
	ASSIGN_OR_RETURN(sprites, LoadClxListOrSheetWithStatus(path));
#else
	if (animFAmt == 1) {
		char path[MaxMpqPathSize];
		*BufCopy(path, "missiles\\", name) = '\0';
		ASSIGN_OR_RETURN(OwnedClxSpriteList spriteList, LoadCl2WithStatus(path, animWidth));
		sprites.emplace(OwnedClxSpriteListOrSheet { std::move(spriteList) });
	} else {
		FileNameGenerator pathGenerator({ "missiles\\", name }, DEVILUTIONX_CL2_EXT);
		ASSIGN_OR_RETURN(OwnedClxSpriteSheet spriteSheet, LoadMultipleCl2Sheet<16>(pathGenerator, animFAmt, animWidth));
		sprites.emplace(OwnedClxSpriteListOrSheet { std::move(spriteSheet) });
	}
#endif
	return {};
}

MissileFileData &GetMissileSpriteData(MissileGraphicID graphicId)
{
	return MissileSpriteData[static_cast<std::underlying_type_t<MissileGraphicID>>(graphicId)];
}

void LoadMissileData()
{
	LoadMissileSpriteData();
	LoadMisdat();
}

const MissileData &GetMissileData(MissileID missileId)
{
	return MissilesData[static_cast<std::underlying_type_t<MissileID>>(missileId)];
}

tl::expected<void, std::string> InitMissileGFX()
{
	if (HeadlessMode)
		return {};

	for (MissileFileData &missileSprite : MissileSpriteData) {
		if (missileSprite.flags == MissileGraphicsFlags::MonsterOwned)
			continue;
		RETURN_IF_ERROR(missileSprite.LoadGFX());
	}
	return {};
}

void FreeMissileGFX()
{
	for (MissileFileData &missileData : MissileSpriteData) {
		missileData.FreeGFX();
	}
}

} // namespace devilution
