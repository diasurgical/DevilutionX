#include "engine/assets.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <vector>

#ifdef USE_SDL3
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#else
#include <SDL.h>
#endif

#include "appfat.h"
#include "game_mode.hpp"
#include "utils/file_util.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/sdl_compat.h"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"

#if defined(_WIN32) && !defined(__UWP__) && !defined(DEVILUTIONX_WINDOWS_NO_WCHAR)
#include <find_steam_game.h>
#endif

#ifndef UNPACKED_MPQS
#include "mpq/mpq_sdl_rwops.hpp"
#endif

namespace devilution {

std::vector<std::string> OverridePaths;
std::map<int, MpqArchiveT, std::greater<>> MpqArchives;
bool HasHellfireMpq;

namespace {

#ifdef UNPACKED_MPQS
char *FindUnpackedMpqFile(char *relativePath)
{
	char *path = nullptr;
	for (const auto &[_, unpackedDir] : MpqArchives) {
		path = relativePath - unpackedDir.size();
		std::memcpy(path, unpackedDir.data(), unpackedDir.size());
		if (FileExists(path)) break;
		path = nullptr;
	}
	return path;
}
#else
bool IsDebugLogging()
{
	return IsLogLevel(LogCategory::Application, SDL_LOG_PRIORITY_DEBUG);
}

SDL_IOStream *OpenOptionalRWops(const std::string &path)
{
	// SDL always logs an error in Debug mode.
	// We check the file presence in Debug mode to avoid this.
	if (IsDebugLogging() && !FileExists(path.c_str()))
		return nullptr;
	return SDL_IOFromFile(path.c_str(), "rb");
};

bool FindMpqFile(std::string_view filename, MpqArchive **archive, uint32_t *fileNumber)
{
	const MpqFileHash fileHash = CalculateMpqFileHash(filename);

	for (auto &[_, mpqArchive] : MpqArchives) {
		if (mpqArchive.GetFileNumber(fileHash, *fileNumber)) {
			*archive = &mpqArchive;
			return true;
		}
	}

	return false;
}

#endif

} // namespace

#ifdef UNPACKED_MPQS
AssetRef FindAsset(std::string_view filename)
{
	AssetRef result;
	if (filename.empty() || filename.back() == '\\')
		return result;
	result.path[0] = '\0';

	char pathBuf[AssetRef::PathBufSize];
	char *const pathEnd = pathBuf + AssetRef::PathBufSize;
	char *const relativePath = &pathBuf[AssetRef::PathBufSize - filename.size() - 1];
	*BufCopy(relativePath, filename) = '\0';

#if !defined(_WIN32) && !defined(__DJGPP__)
	std::replace(relativePath, pathEnd, '\\', '/');
#endif
	// Absolute path:
	if (relativePath[0] == '/') {
		if (FileExists(relativePath)) {
			*BufCopy(result.path, std::string_view(relativePath, filename.size())) = '\0';
		}
		return result;
	}

	// Unpacked MPQ file:
	char *const unpackedMpqPath = FindUnpackedMpqFile(relativePath);
	if (unpackedMpqPath != nullptr) {
		*BufCopy(result.path, std::string_view(unpackedMpqPath, pathEnd - unpackedMpqPath)) = '\0';
		return result;
	}

	// The `/assets` directory next to the devilutionx binary.
	const std::string &assetsPathPrefix = paths::AssetsPath();
	char *assetsPath = relativePath - assetsPathPrefix.size();
	std::memcpy(assetsPath, assetsPathPrefix.data(), assetsPathPrefix.size());
	if (FileExists(assetsPath)) {
		*BufCopy(result.path, std::string_view(assetsPath, pathEnd - assetsPath)) = '\0';
	}
	return result;
}
#else
AssetRef FindAsset(std::string_view filename)
{
	AssetRef result;
	if (filename.empty() || filename.back() == '\\')
		return result;

	std::string relativePath { filename };
#ifndef _WIN32
	std::replace(relativePath.begin(), relativePath.end(), '\\', '/');
#endif

	if (relativePath[0] == '/') {
		result.directHandle = SDL_IOFromFile(relativePath.c_str(), "rb");
		if (result.directHandle != nullptr) {
			return result;
		}
	}

	// Files in the `PrefPath()` directory can override MPQ contents.
	{
		for (const auto &overridePath : OverridePaths) {
			const std::string path = overridePath + relativePath;
			result.directHandle = OpenOptionalRWops(path);
			if (result.directHandle != nullptr) {
				LogVerbose("Loaded MPQ file override: {}", path);
				return result;
			}
		}
	}

	// Look for the file in all the MPQ archives:
	if (FindMpqFile(filename, &result.archive, &result.fileNumber)) {
		result.filename = filename;
		return result;
	}

	// Load from the `/assets` directory next to the devilutionx binary.
	result.directHandle = OpenOptionalRWops(paths::AssetsPath() + relativePath);
	if (result.directHandle != nullptr)
		return result;

#if defined(__ANDROID__) || defined(__APPLE__)
	// Fall back to the bundled assets on supported systems.
	// This is handled by SDL when we pass a relative path.
	if (!paths::AssetsPath().empty()) {
		result.directHandle = SDL_IOFromFile(relativePath.c_str(), "rb");
		if (result.directHandle != nullptr)
			return result;
	}
#endif

	return result;
}
#endif

AssetHandle OpenAsset(AssetRef &&ref, bool threadsafe)
{
#if UNPACKED_MPQS
	return AssetHandle { OpenFile(ref.path, "rb") };
#else
	if (ref.archive != nullptr)
		return AssetHandle { SDL_RWops_FromMpqFile(*ref.archive, ref.fileNumber, ref.filename, threadsafe) };
	if (ref.directHandle != nullptr) {
		// Transfer handle ownership:
		auto *handle = ref.directHandle;
		ref.directHandle = nullptr;
		return AssetHandle { handle };
	}
	return AssetHandle { nullptr };
#endif
}

AssetHandle OpenAsset(std::string_view filename, bool threadsafe)
{
	AssetRef ref = FindAsset(filename);
	if (!ref.ok())
		return AssetHandle {};
	return OpenAsset(std::move(ref), threadsafe);
}

AssetHandle OpenAsset(std::string_view filename, size_t &fileSize, bool threadsafe)
{
	AssetRef ref = FindAsset(filename);
	if (!ref.ok())
		return AssetHandle {};
	fileSize = ref.size();
	return OpenAsset(std::move(ref), threadsafe);
}

SDL_IOStream *OpenAssetAsSdlRwOps(std::string_view filename, bool threadsafe)
{
#ifdef UNPACKED_MPQS
	AssetRef ref = FindAsset(filename);
	if (!ref.ok())
		return nullptr;
	return SDL_IOFromFile(ref.path, "rb");
#else
	return OpenAsset(filename, threadsafe).release();
#endif
}

tl::expected<AssetData, std::string> LoadAsset(std::string_view path)
{
	AssetRef ref = FindAsset(path);
	if (!ref.ok()) {
		return tl::make_unexpected(StrCat("Asset not found: ", path));
	}

	const size_t size = ref.size();
	std::unique_ptr<char[]> data { new char[size] };

	AssetHandle handle = OpenAsset(std::move(ref));
	if (!handle.ok()) {
		return tl::make_unexpected(StrCat("Failed to open asset: ", path, "\n", handle.error()));
	}

	if (size > 0 && !handle.read(data.get(), size)) {
		return tl::make_unexpected(StrCat("Read failed: ", path, "\n", handle.error()));
	}

	return AssetData { std::move(data), size };
}

std::string FailedToOpenFileErrorMessage(std::string_view path, std::string_view error)
{
	return fmt::format(fmt::runtime(_("Failed to open file:\n{:s}\n\n{:s}\n\nThe MPQ file(s) might be damaged. Please check the file integrity.")), path, error);
}

namespace {
#ifdef UNPACKED_MPQS
std::optional<std::string> FindUnpackedMpqData(std::span<const std::string> paths, std::string_view mpqName)
{
	std::string targetPath;
	for (const std::string &path : paths) {
		targetPath.clear();
		targetPath.reserve(path.size() + mpqName.size() + 1);
		targetPath.append(path).append(mpqName) += DirectorySeparator;
		if (FileExists(targetPath)) {
			LogVerbose("  Found unpacked MPQ directory: {}", targetPath);
			return targetPath;
		}
	}
	return std::nullopt;
}

bool FindMPQ(std::span<const std::string> paths, std::string_view mpqName)
{
	return FindUnpackedMpqData(paths, mpqName).has_value();
}

bool LoadMPQ(std::span<const std::string> paths, std::string_view mpqName, int priority)
{
	std::optional<std::string> mpqPath = FindUnpackedMpqData(paths, mpqName);
	if (!mpqPath.has_value()) {
		LogVerbose("Missing: {}", mpqName);
		return false;
	}
	MpqArchives[priority] = *std::move(mpqPath);
	return true;
}
#else
bool FindMPQ(std::span<const std::string> paths, std::string_view mpqName)
{
	std::string mpqAbsPath;
	for (const auto &path : paths) {
		mpqAbsPath = StrCat(path, mpqName, ".mpq");
		if (FileExists(mpqAbsPath)) {
			LogVerbose("  Found: {} in {}", mpqName, path);
			return true;
		}
	}

	return false;
}

bool LoadMPQ(std::span<const std::string> paths, std::string_view mpqName, int priority, std::string_view ext = ".mpq")
{
	std::optional<MpqArchive> archive;
	std::string mpqAbsPath;
	std::int32_t error = 0;
	for (const auto &path : paths) {
		mpqAbsPath = StrCat(path, mpqName, ext);
		archive = MpqArchive::Open(mpqAbsPath.c_str(), error);
		if (archive.has_value()) {
			LogVerbose("  Found: {} in {}", mpqName, path);
			auto [it, inserted] = MpqArchives.emplace(priority, *std::move(archive));
			if (!inserted) {
				LogError("MPQ with priority {} is already registered, skipping {}", priority, mpqName);
			}
			return true;
		}
		if (error != 0) {
			LogError("Error {}: {}", MpqArchive::ErrorMessage(error), mpqAbsPath);
		}
	}
	if (error == 0) {
		LogVerbose("Missing: {}", mpqName);
	}

	return false;
}
#endif

std::vector<std::string> GetMPQSearchPaths()
{
	std::vector<std::string> paths;
	paths.push_back(paths::BasePath());
	paths.push_back(paths::PrefPath());
	if (paths[0] == paths[1])
		paths.pop_back();
	paths.push_back(paths::ConfigPath());
	if (paths[0] == paths[1] || (paths.size() == 3 && (paths[0] == paths[2] || paths[1] == paths[2])))
		paths.pop_back();

#if (defined(__unix__) || defined(__APPLE__)) && !defined(__ANDROID__) && !defined(__DJGPP__)
	// `XDG_DATA_HOME` is usually the root path of `paths::PrefPath()`, so we only
	// add `XDG_DATA_DIRS`.
	const char *xdgDataDirs = std::getenv("XDG_DATA_DIRS");
	if (xdgDataDirs != nullptr) {
		for (const std::string_view path : SplitByChar(xdgDataDirs, ':')) {
			std::string fullPath(path);
			if (!path.empty() && path.back() != '/')
				fullPath += '/';
			fullPath.append("diasurgical/devilutionx/");
			paths.push_back(std::move(fullPath));
		}
	} else {
		paths.emplace_back("/usr/local/share/diasurgical/devilutionx/");
		paths.emplace_back("/usr/share/diasurgical/devilutionx/");
	}
#elif defined(NXDK)
	paths.emplace_back("D:\\");
#elif defined(_WIN32) && !defined(__UWP__) && !defined(DEVILUTIONX_WINDOWS_NO_WCHAR)
	char gogpath[_FSG_PATH_MAX];
	fsg_get_gog_game_path(gogpath, "1412601690");
	if (strlen(gogpath) > 0) {
		paths.emplace_back(std::string(gogpath) + "/");
		paths.emplace_back(std::string(gogpath) + "/hellfire/");
	}
#endif

	if (paths.empty() || !paths.back().empty()) {
		paths.emplace_back(); // PWD
	}

	if (IsLogLevel(LogCategory::Application, SDL_LOG_PRIORITY_VERBOSE)) {
		LogVerbose("Paths:\n    base: {}\n    pref: {}\n  config: {}\n  assets: {}",
		    paths::BasePath(), paths::PrefPath(), paths::ConfigPath(), paths::AssetsPath());

		std::string message;
		for (std::size_t i = 0; i < paths.size(); ++i) {
			message.append(StrCat("\n", LeftPad(i + 1, 6, ' '), ". '", paths[i], "'"));
		}
		LogVerbose("MPQ search paths:{}", message);
	}

	return paths;
}

} // namespace

void LoadCoreArchives()
{
	auto paths = GetMPQSearchPaths();

#if !defined(__ANDROID__) && !defined(__APPLE__) && !defined(__3DS__) && !defined(__SWITCH__)
	// Load devilutionx.mpq first to get the font file for error messages
#ifdef __DJGPP__
	LoadMPQ(paths, "devx", DevilutionXMpqPriority);
#else
	LoadMPQ(paths, "devilutionx", DevilutionXMpqPriority);
#endif
#endif
	LoadMPQ(paths, "fonts", FontMpqPriority); // Extra fonts
	HasHellfireMpq = FindMPQ(paths, "hellfire");
}

void LoadLanguageArchive()
{
	MpqArchives.erase(LangMpqPriority);
	const std::string_view code = GetLanguageCode();
	if (code != "en") {
		LoadMPQ(GetMPQSearchPaths(), code, LangMpqPriority);
	}
}

void LoadGameArchives()
{
	const std::vector<std::string> paths = GetMPQSearchPaths();
	bool haveDiabdat = false;
	bool haveSpawn = false;

#ifndef UNPACKED_MPQS
	// DIABDAT.MPQ is uppercase on the original CD and the GOG version.
	haveDiabdat = LoadMPQ(paths, "DIABDAT", MainMpqPriority, ".MPQ");
#endif

	if (!haveDiabdat) {
		haveDiabdat = LoadMPQ(paths, "diabdat", MainMpqPriority);
		if (!haveDiabdat) {
			gbIsSpawn = haveSpawn = LoadMPQ(paths, "spawn", MainMpqPriority);
		}
	}

	if (!HeadlessMode) {
		if (!haveDiabdat && !haveSpawn) {
			LogError("{}", SDL_GetError());
			InsertCDDlg(_("diabdat.mpq or spawn.mpq"));
		}
	}

	if (forceHellfire && !HasHellfireMpq) {
#ifdef UNPACKED_MPQS
		InsertCDDlg("hellfire");
#else
		InsertCDDlg("hellfire.mpq");
#endif
	}

#ifndef UNPACKED_MPQS
	// In unpacked mode, all the hellfire data is in the hellfire directory.
	LoadMPQ(paths, "hfbard", 8110);
	LoadMPQ(paths, "hfbarb", 8120);
#endif
}

void LoadHellfireArchives()
{
	const std::vector<std::string> paths = GetMPQSearchPaths();
	LoadMPQ(paths, "hellfire", 8000);

#ifdef UNPACKED_MPQS
	const std::string &hellfireDataPath = MpqArchives.at(8000);
	const bool hasMonk = FileExists(hellfireDataPath + "plrgfx/monk/mha/mhaas.clx");
	const bool hasMusic = FileExists(hellfireDataPath + "music/dlvlf.wav")
	    || FileExists(hellfireDataPath + "music/dlvlf.mp3");
	const bool hasVoice = FileExists(hellfireDataPath + "sfx/hellfire/cowsut1.wav")
	    || FileExists(hellfireDataPath + "sfx/hellfire/cowsut1.mp3");
#else
	const bool hasMonk = LoadMPQ(paths, "hfmonk", 8100);
	const bool hasMusic = LoadMPQ(paths, "hfmusic", 8200);
	const bool hasVoice = LoadMPQ(paths, "hfvoice", 8500);
#endif

	if (!hasMonk || !hasMusic || !hasVoice)
		DisplayFatalErrorAndExit(_("Some Hellfire MPQs are missing"), _("Not all Hellfire MPQs were found.\nPlease copy all the hf*.mpq files."));
}

void UnloadModArchives()
{
	OverridePaths.clear();

#ifndef UNPACKED_MPQS
	for (auto it = MpqArchives.begin(); it != MpqArchives.end();) {
		if ((it->first >= 8000 && it->first < 9000) || it->first >= 10000) {
			it = MpqArchives.erase(it); // erase returns the next valid iterator
		} else {
			++it;
		}
	}
#endif
}

void LoadModArchives(std::span<const std::string_view> modnames)
{
	std::string targetPath;
	for (const std::string_view modname : modnames) {
		targetPath = StrCat(paths::PrefPath(), "mods" DIRECTORY_SEPARATOR_STR, modname, DIRECTORY_SEPARATOR_STR);
		if (FileExists(targetPath)) {
			OverridePaths.emplace_back(targetPath);
		}
		targetPath = StrCat(paths::BasePath(), "mods" DIRECTORY_SEPARATOR_STR, modname, DIRECTORY_SEPARATOR_STR);
		if (FileExists(targetPath)) {
			OverridePaths.emplace_back(targetPath);
		}
	}
	OverridePaths.emplace_back(paths::PrefPath());

	int priority = 10000;
	auto paths = GetMPQSearchPaths();
	for (const std::string_view modname : modnames) {
		LoadMPQ(paths, StrCat("mods" DIRECTORY_SEPARATOR_STR, modname), priority);
		priority++;
	}
}

} // namespace devilution
