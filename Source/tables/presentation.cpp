#include "tables/presentation.hpp"

#include "data/file.hpp"
#include "data/value_reader.hpp"

namespace devilution {

void LoadPresentationRules()
{
	Presentation = {};
	constexpr std::string_view Filename = "txtdata\\presentation.tsv";
	auto result = DataFile::load(Filename);
	if (!result.has_value()) {
		if (result.error() != DataFile::Error::NotFound)
			DataFile::reportFatalError(result.error(), Filename);
		return;
	}
	DataFile &dataFile = *result;
	dataFile.skipHeaderOrDie(Filename);
	ValueReader reader { dataFile, Filename };
	reader.readString("startupLogo", Presentation.startupLogo);
	reader.readString("victoryMovie", Presentation.victoryMovie);
	reader.readString("loadingPalette", Presentation.loadingPalette);
	reader.readString("townMusic", Presentation.townMusic);
	reader.readString("skullLeverName", Presentation.skullLeverName);
	reader.readString("bookcaseName", Presentation.bookcaseName);
}

} // namespace devilution
