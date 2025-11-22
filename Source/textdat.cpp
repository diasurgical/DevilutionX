/**
 * @file textdat.cpp
 *
 * Implementation of all dialog texts.
 */
#include "textdat.h"

#include <ankerl/unordered_dense.h>
#include <fmt/core.h>
#include <magic_enum/magic_enum.hpp>

#include "data/file.hpp"
#include "data/record_reader.hpp"
#include "effects.h"
#include "utils/language.h"

namespace devilution {

/* todo: move text out of struct */

/** Contains the data related to each speech ID. */
std::vector<Speech> Speeches;

/** Contains the mapping between text ID strings and indices, used for parsing additional text data. */
ankerl::unordered_dense::map<std::string, int16_t> AdditionalTextIdStringsToIndices;

tl::expected<_speech_id, std::string> ParseSpeechId(std::string_view value)
{
	if (value.empty()) {
		return TEXT_NONE;
	}

	const std::optional<_speech_id> enumValueOpt = magic_enum::enum_cast<_speech_id>(value);
	if (enumValueOpt.has_value()) {
		return enumValueOpt.value();
	}

	const auto findIt = AdditionalTextIdStringsToIndices.find(std::string(value));
	if (findIt != AdditionalTextIdStringsToIndices.end()) {
		return static_cast<_speech_id>(findIt->second);
	}

	return tl::make_unexpected("Invalid value.");
}

namespace {

void LoadTextDatFromFile(DataFile &dataFile, std::string_view filename, bool grow)
{
	dataFile.skipHeaderOrDie(filename);

	if (grow) {
		Speeches.reserve(Speeches.size() + dataFile.numRecords());
	}

	for (DataFileRecord record : dataFile) {
		RecordReader reader { record, filename };

		std::string txtstrid;
		reader.readString("txtstrid", txtstrid);

		if (txtstrid.empty()) {
			continue;
		}

		const std::optional<_speech_id> speechIdEnumValueOpt = magic_enum::enum_cast<_speech_id>(txtstrid);

		if (!speechIdEnumValueOpt.has_value()) {
			const size_t textEntryIndex = Speeches.size();
			const auto [it, inserted] = AdditionalTextIdStringsToIndices.emplace(txtstrid, static_cast<int16_t>(textEntryIndex));
			if (!inserted) {
				DisplayFatalErrorAndExit(_("Loading Text Data Failed"), fmt::format(fmt::runtime(_("A text data entry already exists for ID \"{}\".")), txtstrid));
			}
		}

		// for hardcoded speeches, use their predetermined slot; for non-hardcoded ones, use the slots after that
		Speech &speech = speechIdEnumValueOpt.has_value() ? Speeches[speechIdEnumValueOpt.value()] : Speeches.emplace_back();

		reader.readString("txtstr", speech.txtstr);

		{
			std::string processed;
			processed.reserve(speech.txtstr.size());
			for (size_t i = 0; i < speech.txtstr.size();) {
				if (i + 1 < speech.txtstr.size() && speech.txtstr[i] == '\\' && speech.txtstr[i + 1] == 'n') {
					processed.push_back('\n');
					i += 2;
				} else {
					processed.push_back(speech.txtstr[i]);
					++i;
				}
			}
			speech.txtstr = std::move(processed);
		}

		reader.readBool("scrlltxt", speech.scrlltxt);
		reader.read("sfxnr", speech.sfxnr, ParseSfxId);
	}
}

} // namespace

void LoadTextData()
{
	const std::string_view filename = "txtdata\\text\\textdat.tsv";
	DataFile dataFile = DataFile::loadOrDie(filename);

	Speeches.clear();
	AdditionalTextIdStringsToIndices.clear();
	Speeches.resize(NUM_DEFAULT_TEXT_IDS); // ensure the hardcoded text entry slots are filled
	LoadTextDatFromFile(dataFile, filename, false);

	Speeches.shrink_to_fit();
}

} // namespace devilution
