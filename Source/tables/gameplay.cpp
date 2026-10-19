#include "tables/gameplay.h"

#include "data/file.hpp"
#include "data/value_reader.hpp"
#include "items.h"

namespace devilution {
namespace {

void ReadBoundedInt(ValueReader &reader, std::string_view name, int &value,
    int minimum, int maximum)
{
	reader.readValue(name, value, [minimum, maximum](DataFileField &field, int &out) -> std::expected<void, DataFileField::Error> {
		const auto result = field.parseInt(out);
		if (!result.has_value()) {
			return result;
		}
		if (out < minimum || out > maximum) {
			return std::unexpected(DataFileField::Error::InvalidValue);
		}
		return {};
	});
}

} // namespace

void LoadGameplayRules()
{
	Gameplay = {};
	MaxGold = Gameplay.goldStackLimit;
	constexpr std::string_view Filename = "txtdata\\gameplay.tsv";
	auto result = DataFile::load(Filename);
	if (!result.has_value()) {
		if (result.error() != DataFile::Error::NotFound) {
			DataFile::reportFatalError(result.error(), Filename);
		}
		return;
	}
	DataFile &dataFile = result.value();
	dataFile.skipHeaderOrDie(Filename);
	ValueReader reader { dataFile, Filename };
	ReadBoundedInt(reader, "maxResistance", Gameplay.maxResistance, 0, 100);
	ReadBoundedInt(reader, "goldStackLimit", Gameplay.goldStackLimit, 1, 65000);
	ReadBoundedInt(reader, "vendorValueLimit", Gameplay.vendorValueLimit, 1, 10000000);
	ReadBoundedInt(reader, "boyValueLimit", Gameplay.boyValueLimit, 1, 10000000);
	ReadBoundedInt(reader, "nightmareToHitBonus", Gameplay.nightmareToHitBonus, 0, 1000);
	ReadBoundedInt(reader, "hellToHitBonus", Gameplay.hellToHitBonus, 0, 1000);
	ReadBoundedInt(reader, "nightmareAcBonus", Gameplay.nightmareAcBonus, 0, 1000);
	ReadBoundedInt(reader, "hellAcBonus", Gameplay.hellAcBonus, 0, 1000);
	ReadBoundedInt(reader, "hellHitPointBonus", Gameplay.hellHitPointBonus, 0, 100000);
	ReadBoundedInt(reader, "fireballDamageMultiplier", Gameplay.fireballDamageMultiplier, 1, 100);
	ReadBoundedInt(reader, "fireballDamageBonus", Gameplay.fireballDamageBonus, 0, 10000);
	ReadBoundedInt(reader, "chainLightningLevelDivisor", Gameplay.chainLightningLevelDivisor, 1, 100);
	ReadBoundedInt(reader, "allowMovementDuringDiabloDeath", Gameplay.allowMovementDuringDiabloDeath, 0, 1);
	ReadBoundedInt(reader, "automapStartsActive", Gameplay.automapStartsActive, 0, 1);
	MaxGold = Gameplay.goldStackLimit;
}

} // namespace devilution
