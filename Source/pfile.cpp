/**
 * @file pfile.cpp
 *
 * Implementation of the save game encoding functionality.
 */
#include "pfile.h"

#include <cstdint>
#include <string>
#include <string_view>

#include <ankerl/unordered_dense.h>
#include <expected.hpp>

#ifdef USE_SDL3
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "codec.h"
#include "engine/load_file.hpp"
#include "engine/render/primitive_render.hpp"
#include "game_mode.hpp"
#include "loadsave.h"
#include "menu.h"
#include "mpq/mpq_common.hpp"
#include "pack.h"
#include "playerdat.hpp"
#include "qol/stash.h"
#include "utils/endian_read.hpp"
#include "utils/endian_swap.hpp"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/parse_int.hpp"
#include "utils/paths.h"
#include "utils/sdl_compat.h"
#include "utils/stdcompat/filesystem.hpp"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"
#include "utils/utf8.hpp"

#ifdef UNPACKED_SAVES
#include "utils/file_util.h"
#else
#include "mpq/mpq_reader.hpp"
#endif

namespace devilution {

#define PASSWORD_SPAWN_SINGLE "adslhfb1"
#define PASSWORD_SPAWN_MULTI "lshbkfg1"
#define PASSWORD_SINGLE "xrgyrkj1"
#define PASSWORD_MULTI "szqnlsk1"

bool gbValidSaveFile;

namespace {

/** List of character names for the character selection screen. */
char hero_names[MAX_CHARACTERS][PlayerNameLength];

std::string GetSavePath(uint32_t saveNum, std::string_view savePrefix = {})
{
	return StrCat(paths::PrefPath(), savePrefix,
	    gbIsSpawn
	        ? (gbIsMultiplayer ? "share_" : "spawn_")
	        : (gbIsMultiplayer ? "multi_" : "single_"),
	    saveNum,
#ifdef UNPACKED_SAVES
	    gbIsHellfire ? "_hsv" DIRECTORY_SEPARATOR_STR : "_sv" DIRECTORY_SEPARATOR_STR
#else
	    gbIsHellfire ? ".hsv" : ".sv"
#endif
	);
}

std::string GetStashSavePath()
{
	return StrCat(paths::PrefPath(),
	    gbIsSpawn ? "stash_spawn" : "stash",
#ifdef UNPACKED_SAVES
	    gbIsHellfire ? "_hsv" DIRECTORY_SEPARATOR_STR : "_sv" DIRECTORY_SEPARATOR_STR
#else
	    gbIsHellfire ? ".hsv" : ".sv"
#endif
	);
}

bool GetSaveNames(uint8_t index, std::string_view prefix, char *out)
{
	char suf;
	if (index < giNumberOfLevels)
		suf = 'l';
	else if (index < giNumberOfLevels * 2) {
		index -= giNumberOfLevels;
		suf = 's';
	} else {
		return false;
	}

	*BufCopy(out, prefix, std::string_view(&suf, 1), LeftPad(index, 2, '0')) = '\0';
	return true;
}

bool GetPermSaveNames(uint8_t dwIndex, char *szPerm)
{
	return GetSaveNames(dwIndex, "perm", szPerm);
}

bool GetTempSaveNames(uint8_t dwIndex, char *szTemp)
{
	return GetSaveNames(dwIndex, "temp", szTemp);
}

void RenameTempToPerm(SaveWriter &saveWriter)
{
	char szTemp[MaxMpqPathSize];
	char szPerm[MaxMpqPathSize];

	uint32_t dwIndex = 0;
	while (GetTempSaveNames(dwIndex, szTemp)) {
		[[maybe_unused]] const bool result = GetPermSaveNames(dwIndex, szPerm); // DO NOT PUT DIRECTLY INTO ASSERT!
		assert(result);
		dwIndex++;
		if (saveWriter.HasFile(szTemp)) {
			if (saveWriter.HasFile(szPerm))
				saveWriter.RemoveHashEntry(szPerm);
			saveWriter.RenameFile(szTemp, szPerm);
		}
	}
	assert(!GetPermSaveNames(dwIndex, szPerm));
}

bool ReadHero(SaveReader &archive, PlayerPack *pPack)
{
	size_t read;

	auto buf = ReadArchive(archive, "hero", &read);
	if (buf == nullptr)
		return false;

	bool ret = false;
	if (read == sizeof(*pPack)) {
		memcpy(pPack, buf.get(), sizeof(*pPack));
		ret = true;
	}

	return ret;
}

void EncodeHero(SaveWriter &saveWriter, const PlayerPack *pack)
{
	const size_t packedLen = codec_get_encoded_len(sizeof(*pack));
	const std::unique_ptr<std::byte[]> packed { new std::byte[packedLen] };

	memcpy(packed.get(), pack, sizeof(*pack));
	codec_encode(packed.get(), sizeof(*pack), packedLen, pfile_get_password());
	saveWriter.WriteFile("hero", packed.get(), packedLen);
}

SaveWriter GetSaveWriter(uint32_t saveNum)
{
	return SaveWriter(GetSavePath(saveNum));
}

SaveWriter GetStashWriter()
{
	return SaveWriter(GetStashSavePath());
}

#ifndef DISABLE_DEMOMODE
void CopySaveFile(uint32_t saveNum, std::string targetPath)
{
	const std::string savePath = GetSavePath(saveNum);
#if defined(UNPACKED_SAVES)
#ifdef DVL_NO_FILESYSTEM
#error "UNPACKED_SAVES requires either DISABLE_DEMOMODE or C++17 <filesystem>"
#endif
	if (!targetPath.empty()) {
		CreateDir(targetPath.c_str());
	}
	for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(savePath)) {
		CopyFileOverwrite(entry.path().string().c_str(), (targetPath + entry.path().filename().string()).c_str());
	}
#else
	CopyFileOverwrite(savePath.c_str(), targetPath.c_str());
#endif
}
#endif

void Game2UiPlayer(const Player &player, _uiheroinfo *heroinfo, bool bHasSaveFile)
{
	CopyUtf8(heroinfo->name, player._pName, sizeof(heroinfo->name));
	heroinfo->level = player.getCharacterLevel();
	heroinfo->heroclass = player._pClass;
	heroinfo->strength = player._pStrength;
	heroinfo->magic = player._pMagic;
	heroinfo->dexterity = player._pDexterity;
	heroinfo->vitality = player._pVitality;
	heroinfo->hassaved = bHasSaveFile;
	heroinfo->herorank = player.pDiabloKillLevel;
	heroinfo->spawned = gbIsSpawn;
}

bool GetFileName(uint8_t lvl, char *dst)
{
	if (gbIsMultiplayer) {
		if (lvl != 0)
			return false;
		memcpy(dst, "hero", 5);
		return true;
	}
	if (GetPermSaveNames(lvl, dst)) {
		return true;
	}
	if (lvl == giNumberOfLevels * 2) {
		memcpy(dst, "game", 5);
		return true;
	}
	if (lvl == giNumberOfLevels * 2 + 1) {
		memcpy(dst, "hero", 5);
		return true;
	}
	return false;
}

bool ArchiveContainsGame(SaveReader &hsArchive)
{
	if (gbIsMultiplayer)
		return false;

	auto gameData = ReadArchive(hsArchive, "game");
	if (gameData == nullptr)
		return false;

	const uint32_t hdr = LoadLE32(gameData.get());

	return IsHeaderValid(hdr);
}

std::optional<SaveReader> CreateSaveReader(std::string &&path)
{
#ifdef UNPACKED_SAVES
	if (!FileExists(path))
		return std::nullopt;
	return SaveReader(std::move(path));
#else
	std::int32_t error;
	return MpqArchive::Open(path.c_str(), error);
#endif
}

#ifndef DISABLE_DEMOMODE
struct CompareInfo {
	std::unique_ptr<std::byte[]> &data;
	size_t currentPosition;
	size_t size;
	bool isTownLevel;
	bool dataExists;
};

struct CompareCounter {
	int reference;
	int actual;
	int max() const
	{
		return std::max(reference, actual);
	}
	void checkIfDataExists(int count, CompareInfo &compareInfoReference, CompareInfo &compareInfoActual) const
	{
		if (reference == count)
			compareInfoReference.dataExists = false;
		if (actual == count)
			compareInfoActual.dataExists = false;
	}
};

inline bool string_ends_with(std::string_view value, std::string_view suffix)
{
	if (suffix.size() > value.size())
		return false;
	return std::equal(suffix.rbegin(), suffix.rend(), value.rbegin());
}

void CreateDetailDiffs(std::string_view prefix, std::string_view memoryMapFile, CompareInfo &compareInfoReference, CompareInfo &compareInfoActual, ankerl::unordered_dense::segmented_map<std::string, size_t> &foundDiffs)
{
	// Note: Detail diffs are currently only supported in unit tests
	const std::string memoryMapFileAssetName = StrCat(paths::BasePath(), "/test/fixtures/memory_map/", memoryMapFile, ".txt");

	SDL_IOStream *handle = SDL_IOFromFile(memoryMapFileAssetName.c_str(), "r");
	if (handle == nullptr) {
		app_fatal(StrCat("MemoryMapFile ", memoryMapFile, " is missing"));
		return;
	}

	const size_t readBytes = static_cast<size_t>(SDL_GetIOSize(handle));
	const std::unique_ptr<std::byte[]> memoryMapFileData { new std::byte[readBytes] };
	SDL_ReadIO(handle, memoryMapFileData.get(), readBytes);
	SDL_CloseIO(handle);

	const std::string_view buffer(reinterpret_cast<const char *>(memoryMapFileData.get()), readBytes);

	ankerl::unordered_dense::segmented_map<std::string, CompareCounter> counter;

	auto getCounter = [&](const std::string &counterAsString) {
		auto it = counter.find(counterAsString);
		if (it != counter.end())
			return it->second;
		const ParseIntResult<int> countFromMapFile = ParseInt<int>(counterAsString);
		if (!countFromMapFile.has_value())
			app_fatal(StrCat("Failed to parse ", counterAsString, " as int"));
		return CompareCounter { countFromMapFile.value(), countFromMapFile.value() };
	};
	auto addDiff = [&](const std::string &diffKey) {
		auto it = foundDiffs.find(diffKey);
		if (it == foundDiffs.end()) {
			foundDiffs.insert_or_assign(diffKey, 1);
		} else {
			foundDiffs.insert_or_assign(diffKey, it->second + 1);
		}
	};

	auto compareBytes = [&](size_t countBytes) {
		if (compareInfoReference.dataExists && compareInfoReference.currentPosition + countBytes > compareInfoReference.size)
			app_fatal(StrCat("Comparison failed. Not enough bytes in reference to compare. Location: ", prefix));
		if (compareInfoActual.dataExists && compareInfoActual.currentPosition + countBytes > compareInfoActual.size)
			app_fatal(StrCat("Comparison failed. Not enough bytes in actual to compare. Location: ", prefix));
		bool result = true;
		if (compareInfoReference.dataExists && compareInfoActual.dataExists)
			result = memcmp(compareInfoReference.data.get() + compareInfoReference.currentPosition, compareInfoActual.data.get() + compareInfoActual.currentPosition, countBytes) == 0;
		if (compareInfoReference.dataExists)
			compareInfoReference.currentPosition += countBytes;
		if (compareInfoActual.dataExists)
			compareInfoActual.currentPosition += countBytes;
		return result;
	};

	auto read32BitInt = [&](CompareInfo &compareInfo, bool useLE) {
		int32_t value = 0;
		if (!compareInfo.dataExists)
			return value;
		if (compareInfo.currentPosition + sizeof(value) > compareInfo.size)
			app_fatal("read32BitInt failed. Too less bytes to read.");
		memcpy(&value, compareInfo.data.get() + compareInfo.currentPosition, sizeof(value));
		if (useLE)
			value = Swap32LE(value);
		else
			value = Swap32BE(value);
		return value;
	};

	for (std::string_view line : SplitByChar(buffer, '\n')) {
		if (!line.empty() && line.back() == '\r')
			line.remove_suffix(1);
		if (line.empty())
			continue;
		const auto tokens = SplitByChar(line, ' ');
		auto it = tokens.begin();
		const auto end = tokens.end();
		if (it == end)
			continue;

		std::string_view command = *it;

		const bool dataExistsReference = compareInfoReference.dataExists;
		const bool dataExistsActual = compareInfoActual.dataExists;

		if (string_ends_with(command, "_HF")) {
			if (!gbIsHellfire)
				continue;
			command.remove_suffix(3);
		}
		if (string_ends_with(command, "_DA")) {
			if (gbIsHellfire)
				continue;
			command.remove_suffix(3);
		}
		if (string_ends_with(command, "_DL")) {
			if (compareInfoReference.isTownLevel && compareInfoActual.isTownLevel)
				continue;
			if (compareInfoReference.isTownLevel)
				compareInfoReference.dataExists = false;
			if (compareInfoActual.isTownLevel)
				compareInfoActual.dataExists = false;
			command.remove_suffix(3);
		}
		if (command == "R" || command == "LT" || command == "LC" || command == "LC_LE") {
			const auto bitsAsString = std::string(*++it);
			const auto comment = std::string(*++it);
			const ParseIntResult<size_t> parsedBytes = ParseInt<size_t>(bitsAsString);
			if (!parsedBytes.has_value())
				app_fatal(StrCat("Failed to parse ", bitsAsString, " as size_t"));
			const size_t bytes = static_cast<size_t>(parsedBytes.value() / 8);

			if (command == "LT") {
				const int32_t valueReference = read32BitInt(compareInfoReference, false);
				const int32_t valueActual = read32BitInt(compareInfoActual, false);
				assert(sizeof(valueReference) == bytes);
				compareInfoReference.isTownLevel = valueReference == 0;
				compareInfoActual.isTownLevel = valueActual == 0;
			}
			if (command == "LC" || command == "LC_LE") {
				const int32_t valueReference = read32BitInt(compareInfoReference, command == "LC_LE");
				const int32_t valueActual = read32BitInt(compareInfoActual, command == "LC_LE");
				assert(sizeof(valueReference) == bytes);
				counter.insert_or_assign(std::string(comment), CompareCounter { valueReference, valueActual });
			}

			if (!compareBytes(bytes)) {
				const std::string diffKey = StrCat(prefix, ".", comment);
				addDiff(diffKey);
			}
		} else if (command == "M") {
			const auto countAsString = std::string(*++it);
			const auto bitsAsString = std::string(*++it);
			const std::string_view comment = *++it;

			const CompareCounter count = getCounter(countAsString);
			const ParseIntResult<size_t> parsedBytes = ParseInt<size_t>(bitsAsString);
			if (!parsedBytes.has_value())
				app_fatal(StrCat("Failed to parse ", bitsAsString, " as size_t"));
			const size_t bytes = static_cast<size_t>(parsedBytes.value() / 8);
			for (int i = 0; i < count.max(); i++) {
				count.checkIfDataExists(i, compareInfoReference, compareInfoActual);
				if (!compareBytes(bytes)) {
					const std::string diffKey = StrCat(prefix, ".", comment);
					addDiff(diffKey);
				}
			}
		} else if (command == "C") {
			const auto countAsString = std::string(*++it);
			auto subMemoryMapFile = std::string(*++it);
			const auto comment = std::string(*++it);

			const CompareCounter count = getCounter(countAsString);
			subMemoryMapFile.erase(std::remove(subMemoryMapFile.begin(), subMemoryMapFile.end(), '\r'), subMemoryMapFile.end());
			for (int i = 0; i < count.max(); i++) {
				count.checkIfDataExists(i, compareInfoReference, compareInfoActual);
				const std::string subPrefix = StrCat(prefix, ".", comment);
				CreateDetailDiffs(subPrefix, subMemoryMapFile, compareInfoReference, compareInfoActual, foundDiffs);
			}
		}

		compareInfoReference.dataExists = dataExistsReference;
		compareInfoActual.dataExists = dataExistsActual;
	}
}

struct CompareTargets {
	std::string fileName;
	std::string memoryMapFileName;
	bool isTownLevel;
};

HeroCompareResult CompareSaves(const std::string &actualSavePath, const std::string &referenceSavePath, bool logDetails)
{
	std::vector<CompareTargets> possibleFileToCheck;
	possibleFileToCheck.push_back({ "hero", "hero", false });
	possibleFileToCheck.push_back({ "game", "game", false });
	possibleFileToCheck.push_back({ "additionalMissiles", "additionalMissiles", false });
	char szPerm[MaxMpqPathSize];
	for (int i = 0; GetPermSaveNames(i, szPerm); i++) {
		possibleFileToCheck.push_back({ std::string(szPerm), "level", i == 0 });
	}

	SaveReader actualArchive = *CreateSaveReader(std::string(actualSavePath));
	SaveReader referenceArchive = *CreateSaveReader(std::string(referenceSavePath));

	bool compareResult = true;
	std::string message;
	for (const auto &compareTarget : possibleFileToCheck) {
		size_t fileSizeActual = 0;
		auto fileDataActual = ReadArchive(actualArchive, compareTarget.fileName.c_str(), &fileSizeActual);
		size_t fileSizeReference = 0;
		auto fileDataReference = ReadArchive(referenceArchive, compareTarget.fileName.c_str(), &fileSizeReference);
		if (fileDataActual.get() == nullptr && fileDataReference.get() == nullptr) {
			continue;
		}
		if (fileSizeActual == fileSizeReference && memcmp(fileDataReference.get(), fileDataActual.get(), fileSizeActual) == 0)
			continue;
		compareResult = false;
		if (!message.empty())
			message.append("\n");
		if (fileSizeActual != fileSizeReference)
			StrAppend(message, "file \"", compareTarget.fileName, "\" is different size. Expected: ", fileSizeReference, " Actual: ", fileSizeActual);
		else
			StrAppend(message, "file \"", compareTarget.fileName, "\" has different content.");
		if (!logDetails)
			continue;
		ankerl::unordered_dense::segmented_map<std::string, size_t> foundDiffs;
		CompareInfo compareInfoReference = { fileDataReference, 0, fileSizeReference, compareTarget.isTownLevel, fileSizeReference != 0 };
		CompareInfo compareInfoActual = { fileDataActual, 0, fileSizeActual, compareTarget.isTownLevel, fileSizeActual != 0 };
		CreateDetailDiffs(compareTarget.fileName, compareTarget.memoryMapFileName, compareInfoReference, compareInfoActual, foundDiffs);
		if (compareInfoReference.currentPosition != fileSizeReference)
			app_fatal(StrCat("Comparison failed. Uncompared bytes in reference. File: ", compareTarget.fileName));
		if (compareInfoActual.currentPosition != fileSizeActual)
			app_fatal(StrCat("Comparison failed. Uncompared bytes in actual. File: ", compareTarget.fileName));
		for (const auto &[location, count] : foundDiffs) {
			StrAppend(message, "\nDiff found in ", location, " count: ", count);
		}
	}
	return { compareResult ? HeroCompareResult::Same : HeroCompareResult::Difference, message };
}
#endif // !DISABLE_DEMOMODE

void pfile_write_hero(SaveWriter &saveWriter, bool writeGameData)
{
	if (writeGameData) {
		SaveGameData(saveWriter);
		RenameTempToPerm(saveWriter);
	}
	PlayerPack pkplr;
	Player &myPlayer = *MyPlayer;

	PackPlayer(pkplr, myPlayer);
	EncodeHero(saveWriter, &pkplr);
	if (!gbVanilla) {
		SaveHotkeys(saveWriter, myPlayer);
		SaveHeroItems(saveWriter, myPlayer);
	}
}

void RemoveAllInvalidItems(Player &player)
{
	for (int i = 0; i < NUM_INVLOC; i++)
		RemoveInvalidItem(player.InvBody[i]);
	for (int i = 0; i < player._pNumInv; i++)
		RemoveInvalidItem(player.InvList[i]);
	for (int i = 0; i < MaxBeltItems; i++)
		RemoveInvalidItem(player.SpdList[i]);
	RemoveEmptyInventory(player);
}

} // namespace

#ifdef UNPACKED_SAVES
std::unique_ptr<std::byte[]> SaveReader::ReadFile(const char *filename, std::size_t &fileSize, int32_t &error)
{
	std::unique_ptr<std::byte[]> result;
	error = 0;
	const std::string path = dir_ + filename;
	uintmax_t size;
	if (!GetFileSize(path.c_str(), &size)) {
		error = 1;
		return nullptr;
	}
	fileSize = size;
	FILE *file = OpenFile(path.c_str(), "rb");
	if (file == nullptr) {
		error = 1;
		return nullptr;
	}
	result.reset(new std::byte[size]);
	if (std::fread(result.get(), size, 1, file) != 1) {
		std::fclose(file);
		error = 1;
		return nullptr;
	}
	std::fclose(file);
	return result;
}

bool SaveWriter::WriteFile(const char *filename, const std::byte *data, size_t size)
{
	const std::string path = dir_ + filename;
	FILE *file = OpenFile(path.c_str(), "wb");
	if (file == nullptr) {
		return false;
	}
	if (std::fwrite(data, size, 1, file) != 1) {
		std::fclose(file);
		return false;
	}
	std::fclose(file);
	return true;
}

void SaveWriter::RemoveHashEntries(bool (*fnGetName)(uint8_t, char *))
{
	char pszFileName[MaxMpqPathSize];

	for (uint8_t i = 0; fnGetName(i, pszFileName); i++) {
		RemoveHashEntry(pszFileName);
	}
}
#endif

std::optional<SaveReader> OpenSaveArchive(uint32_t saveNum)
{
	return CreateSaveReader(GetSavePath(saveNum));
}

std::optional<SaveReader> OpenStashArchive()
{
	return CreateSaveReader(GetStashSavePath());
}

std::unique_ptr<std::byte[]> ReadArchive(SaveReader &archive, const char *pszName, size_t *pdwLen)
{
	int32_t error;
	std::size_t length;

	std::unique_ptr<std::byte[]> result = archive.ReadFile(pszName, length, error);
	if (error != 0)
		return nullptr;

	const std::size_t decodedLength = codec_decode(result.get(), length, pfile_get_password());
	if (decodedLength == 0)
		return nullptr;

	if (pdwLen != nullptr)
		*pdwLen = decodedLength;

	return result;
}

const char *pfile_get_password()
{
	if (gbIsSpawn)
		return gbIsMultiplayer ? PASSWORD_SPAWN_MULTI : PASSWORD_SPAWN_SINGLE;
	return gbIsMultiplayer ? PASSWORD_MULTI : PASSWORD_SINGLE;
}

void pfile_write_hero(bool writeGameData)
{
	SaveWriter saveWriter = GetSaveWriter(gSaveNumber);
	pfile_write_hero(saveWriter, writeGameData);
}

#ifndef DISABLE_DEMOMODE
void pfile_write_hero_demo(int demo)
{
	const std::string savePath = GetSavePath(gSaveNumber, StrCat("demo_", demo, "_reference_"));
	CopySaveFile(gSaveNumber, savePath);
	auto saveWriter = SaveWriter(savePath.c_str());
	pfile_write_hero(saveWriter, true);
}

HeroCompareResult pfile_compare_hero_demo(int demo, bool logDetails)
{
	const std::string referenceSavePath = GetSavePath(gSaveNumber, StrCat("demo_", demo, "_reference_"));

	if (!FileExists(referenceSavePath.c_str()))
		return { HeroCompareResult::ReferenceNotFound, {} };

	const std::string actualSavePath = GetSavePath(gSaveNumber, StrCat("demo_", demo, "_actual_"));
	{
		CopySaveFile(gSaveNumber, actualSavePath);
		SaveWriter saveWriter(actualSavePath.c_str());
		pfile_write_hero(saveWriter, true);
	}

	return CompareSaves(actualSavePath, referenceSavePath, logDetails);
}
#endif

void sfile_write_stash()
{
	if (!Stash.dirty)
		return;

	SaveWriter stashWriter = GetStashWriter();

	SaveStash(stashWriter);

	Stash.dirty = false;
}

bool pfile_ui_set_hero_infos(bool (*uiAddHeroInfo)(_uiheroinfo *))
{
	memset(hero_names, 0, sizeof(hero_names));

	for (uint32_t i = 0; i < MAX_CHARACTERS; i++) {
		std::optional<SaveReader> archive = OpenSaveArchive(i);
		if (archive) {
			PlayerPack pkplr;
			if (ReadHero(*archive, &pkplr)) {
				_uiheroinfo uihero;
				uihero.saveNumber = i;
				strcpy(hero_names[i], pkplr.pName);
				const bool hasSaveGame = ArchiveContainsGame(*archive);
				if (hasSaveGame)
					pkplr.bIsHellfire = gbIsHellfireSaveGame ? 1 : 0;

				Player &player = Players[0];

				UnPackPlayer(pkplr, player);
				LoadHeroItems(player);
				RemoveAllInvalidItems(player);
				CalcPlrInv(player, false);

				Game2UiPlayer(player, &uihero, hasSaveGame);
				uiAddHeroInfo(&uihero);
			}
		}
	}

	return true;
}

void pfile_ui_set_class_stats(HeroClass playerClass, _uidefaultstats *classStats)
{
	const ClassAttributes &classAttributes = GetClassAttributes(playerClass);
	classStats->strength = classAttributes.baseStr;
	classStats->magic = classAttributes.baseMag;
	classStats->dexterity = classAttributes.baseDex;
	classStats->vitality = classAttributes.baseVit;
}

uint32_t pfile_ui_get_first_unused_save_num()
{
	uint32_t saveNum;
	for (saveNum = 0; saveNum < MAX_CHARACTERS; saveNum++) {
		if (hero_names[saveNum][0] == '\0')
			break;
	}
	return saveNum;
}

bool pfile_ui_save_create(_uiheroinfo *heroinfo)
{
	PlayerPack pkplr;

	const uint32_t saveNum = heroinfo->saveNumber;
	if (saveNum >= MAX_CHARACTERS)
		return false;
	heroinfo->saveNumber = saveNum;

	giNumberOfLevels = gbIsHellfire ? 25 : 17;

	SaveWriter saveWriter = GetSaveWriter(saveNum);
	saveWriter.RemoveHashEntries(GetFileName);
	CopyUtf8(hero_names[saveNum], heroinfo->name, sizeof(hero_names[saveNum]));

	Player &player = Players[0];
	CreatePlayer(player, heroinfo->heroclass);
	CopyUtf8(player._pName, heroinfo->name, PlayerNameLength);
	PackPlayer(pkplr, player);
	EncodeHero(saveWriter, &pkplr);
	Game2UiPlayer(player, heroinfo, false);
	if (!gbVanilla) {
		SaveHotkeys(saveWriter, player);
		SaveHeroItems(saveWriter, player);
	}

	return true;
}

bool pfile_delete_save(_uiheroinfo *heroInfo)
{
	const uint32_t saveNum = heroInfo->saveNumber;
	if (saveNum < MAX_CHARACTERS) {
		hero_names[saveNum][0] = '\0';
		RemoveFile(GetSavePath(saveNum).c_str());
	}
	return true;
}

void pfile_read_player_from_save(uint32_t saveNum, Player &player)
{
	PlayerPack pkplr;
	{
		std::optional<SaveReader> archive = OpenSaveArchive(saveNum);
		if (!archive)
			app_fatal(_("Unable to open archive"));
		if (!ReadHero(*archive, &pkplr))
			app_fatal(_("Unable to load character"));

		gbValidSaveFile = ArchiveContainsGame(*archive);
		if (gbValidSaveFile)
			pkplr.bIsHellfire = gbIsHellfireSaveGame ? 1 : 0;
	}

	UnPackPlayer(pkplr, player);
	LoadHeroItems(player);
	RemoveAllInvalidItems(player);
	CalcPlrInv(player, false);
}

void pfile_save_level()
{
	SaveWriter saveWriter = GetSaveWriter(gSaveNumber);
	SaveLevel(saveWriter);
}

tl::expected<void, std::string> pfile_convert_levels()
{
	SaveWriter saveWriter = GetSaveWriter(gSaveNumber);
	return ConvertLevels(saveWriter);
}

void pfile_remove_temp_files()
{
	if (gbIsMultiplayer)
		return;

	SaveWriter saveWriter = GetSaveWriter(gSaveNumber);
	saveWriter.RemoveHashEntries(GetTempSaveNames);
}

void pfile_update(bool forceSave)
{
	static Uint32 prevTick;

	if (!gbIsMultiplayer)
		return;

	const Uint32 tick = SDL_GetTicks();
	if (!forceSave && tick - prevTick <= 60000)
		return;

	prevTick = tick;
	pfile_write_hero();
	sfile_write_stash();
}

} // namespace devilution
