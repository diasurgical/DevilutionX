#include "engine/assets.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>

#include "appfat.h"
#include "game_mode.hpp"
#include "utils/file_util.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"

#if defined(_WIN32) && !defined(__UWP__) && !defined(DEVILUTIONX_WINDOWS_NO_WCHAR)
#include <find_steam_game.h>
#endif

#ifndef UNPACKED_MPQS
#include "mpq/mpq_sdl_rwops.hpp"
#endif

namespace devilution {

#ifdef UNPACKED_MPQS
std::optional<std::string> spawn_data_path;
std::optional<std::string> diabdat_data_path;
std::optional<std::string> hellfire_data_path;
std::optional<std::string> font_data_path;
std::optional<std::string> lang_data_path;
#else
std::optional<MpqArchive> spawn_mpq;
std::optional<MpqArchive> diabdat_mpq;
std::optional<MpqArchive> hellfire_mpq;
std::optional<MpqArchive> hfmonk_mpq;
std::optional<MpqArchive> hfbard_mpq;
std::optional<MpqArchive> hfbarb_mpq;
std::optional<MpqArchive> hfmusic_mpq;
std::optional<MpqArchive> hfvoice_mpq;
std::optional<MpqArchive> devilutionx_mpq;
std::optional<MpqArchive> lang_mpq;
std::optional<MpqArchive> font_mpq;
#endif

namespace {

#ifdef UNPACKED_MPQS
char *FindUnpackedMpqFile(char *relativePath)
{
	char *path = nullptr;
	const auto at = [&](const std::optional<std::string> &unpackedDir) -> bool {
		if (!unpackedDir)
			return false;
		path = relativePath - unpackedDir->size();
		std::memcpy(path, unpackedDir->data(), unpackedDir->size());
		if (FileExists(path))
			return true;
		path = nullptr;
		return false;
	};
	at(font_data_path) || at(lang_data_path)
	    || (gbIsHellfire && at(hellfire_data_path))
	    || at(spawn_data_path) || at(diabdat_data_path);
	return path;
}
#else
bool IsDebugLogging()
{
	return SDL_LogGetPriority(SDL_LOG_CATEGORY_APPLICATION) <= SDL_LOG_PRIORITY_DEBUG;
}

SDL_RWops *OpenOptionalRWops(const std::string &path)
{
	// SDL always logs an error in Debug mode.
	// We check the file presence in Debug mode to avoid this.
	if (IsDebugLogging() && !FileExists(path.c_str()))
		return nullptr;
	return SDL_RWFromFile(path.c_str(), "rb");
};

bool FindMpqFile(std::string_view filename, MpqArchive **archive, uint32_t *fileNumber)
{
	const MpqFileHash fileHash = CalculateMpqFileHash(filename);
	const auto at = [=](std::optional<MpqArchive> &src) -> bool {
		if (src && src->GetFileNumber(fileHash, *fileNumber)) {
			*archive = &(*src);
			return true;
		}
		return false;
	};

	return at(font_mpq) || at(lang_mpq) || at(devilutionx_mpq)
	    || (gbIsHellfire && (at(hfvoice_mpq) || at(hfmusic_mpq) || at(hfbarb_mpq) || at(hfbard_mpq) || at(hfmonk_mpq) || at(hellfire_mpq))) || at(spawn_mpq) || at(diabdat_mpq);
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

#ifndef _WIN32
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
		result.directHandle = SDL_RWFromFile(relativePath.c_str(), "rb");
		if (result.directHandle != nullptr) {
			return result;
		}
	}

	// Files in the `PrefPath()` directory can override MPQ contents.
	{
		const std::string path = paths::PrefPath() + relativePath;
		result.directHandle = OpenOptionalRWops(path);
		if (result.directHandle != nullptr) {
			LogVerbose("Loaded MPQ file override: {}", path);
			return result;
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
		result.directHandle = SDL_RWFromFile(relativePath.c_str(), "rb");
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
		SDL_RWops *handle = ref.directHandle;
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

SDL_RWops *OpenAssetAsSdlRwOps(std::string_view filename, bool threadsafe)
{
#ifdef UNPACKED_MPQS
	AssetRef ref = FindAsset(filename);
	if (!ref.ok())
		return nullptr;
	return SDL_RWFromFile(ref.path, "rb");
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
std::optional<std::string> FindUnpackedMpqData(const std::vector<std::string> &paths, std::string_view mpqName)
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
#else
std::optional<MpqArchive> TryLoadMPQVariant(const std::vector<std::string> &paths, std::string mpqName, std::int32_t &error)
{
	std::optional<MpqArchive> archive;
	std::string mpqAbsPath;

	for (const auto &path : paths) {
		mpqAbsPath = path + mpqName;
		if ((archive = MpqArchive::Open(mpqAbsPath.c_str(), error))) {
			LogVerbose("  Found: {} in {}", mpqName, path);
			return archive;
		}
		if (error != 0) {
			LogError("Error {}: {}", MpqArchive::ErrorMessage(error), mpqAbsPath);
		}
	}
	return std::nullopt;
}

std::optional<MpqArchive> LoadMPQ(const std::vector<std::string> &paths, std::string_view mpqName)
{
	std::int32_t error = 0;

	// Try lowercase (original name)
	if (auto archive = TryLoadMPQVariant(paths, std::string(mpqName), error))
		return archive;

	// Try UPPERCASE
	std::string upper(mpqName);
	std::transform(upper.begin(), upper.end(), upper.begin(), ::toupper);
	if (auto archive = TryLoadMPQVariant(paths, upper, error))
		return archive;

	// Try Capitalized
	std::string capitalized = std::string(mpqName);
	capitalized[0] = static_cast<char>(::toupper(capitalized[0]));
	std::transform(capitalized.begin() + 1, capitalized.end(), capitalized.begin() + 1, ::tolower);
	if (auto archive = TryLoadMPQVariant(paths, capitalized, error))
		return archive;

	// Additional check: filename is all uppercase but extension is lowercase,
	// and filename is all lowercase but extension is uppercase.
	{
		std::string original(mpqName);
		std::size_t dot = original.find_last_of('.');
		if (dot != std::string::npos) {
			std::string base = original.substr(0, dot);
			std::string ext = original.substr(dot); // includes the dot

			// Variant: base uppercase, extension lowercase (e.g., HELLFIRE.mpq)
			std::string variantA = base;
			std::transform(variantA.begin(), variantA.end(), variantA.begin(), ::toupper);
			std::string extA = ext;
			std::transform(extA.begin(), extA.end(), extA.begin(), ::tolower);
			variantA += extA;
			if (auto archive = TryLoadMPQVariant(paths, variantA, error))
				return archive;

			// Variant: base lowercase, extension uppercase (e.g., hellfire.MPQ)
			std::string variantB = base;
			std::transform(variantB.begin(), variantB.end(), variantB.begin(), ::tolower);
			std::string extB = ext;
			std::transform(extB.begin(), extB.end(), extB.begin(), ::toupper);
			variantB += extB;
			if (auto archive = TryLoadMPQVariant(paths, variantB, error))
				return archive;
		}
	}

	if (error == 0) {
		LogVerbose("Missing: {}", mpqName);
	}

	return std::nullopt;
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

#if (defined(__unix__) || defined(__APPLE__)) && !defined(__ANDROID__)
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

	if (SDL_LOG_PRIORITY_VERBOSE >= SDL_LogGetPriority(SDL_LOG_CATEGORY_APPLICATION)) {
		LogVerbose("Paths:\n    base: {}\n    pref: {}\n  config: {}\n  assets: {}",
		    paths::BasePath(), paths::PrefPath(), paths::ConfigPath(), paths::AssetsPath());

		std::string message;
		for (std::size_t i = 0; i < paths.size(); ++i) {
			message.append(fmt::format("\n{:6d}. '{}'", i + 1, paths[i]));
		}
		LogVerbose("MPQ search paths:{}", message);
	}

	return paths;
}

} // namespace

void LoadCoreArchives()
{
	auto paths = GetMPQSearchPaths();

#ifdef UNPACKED_MPQS
	font_data_path = FindUnpackedMpqData(paths, "fonts");
#else // !UNPACKED_MPQS
#if !defined(__ANDROID__) && !defined(__APPLE__) && !defined(__3DS__) && !defined(__SWITCH__)
	// Load devilutionx.mpq first to get the font file for error messages
	devilutionx_mpq = LoadMPQ(paths, "devilutionx.mpq");
#endif
	font_mpq = LoadMPQ(paths, "fonts.mpq"); // Extra fonts
#endif
}

void LoadLanguageArchive()
{
#ifdef UNPACKED_MPQS
	lang_data_path = std::nullopt;
#else
	lang_mpq = std::nullopt;
#endif

	std::string_view code = GetLanguageCode();
	if (code != "en") {
		std::string langMpqName { code };
#ifdef UNPACKED_MPQS
		lang_data_path = FindUnpackedMpqData(GetMPQSearchPaths(), langMpqName);
#else
		langMpqName.append(".mpq");
		lang_mpq = LoadMPQ(GetMPQSearchPaths(), langMpqName);
#endif
	}
}

void LoadGameArchives()
{
	auto paths = GetMPQSearchPaths();
#ifdef UNPACKED_MPQS
	diabdat_data_path = FindUnpackedMpqData(paths, "diabdat");
	if (!diabdat_data_path) {
		spawn_data_path = FindUnpackedMpqData(paths, "spawn");
		if (spawn_data_path)
			gbIsSpawn = true;
	}
	if (!HeadlessMode) {
		AssetRef ref = FindAsset("ui_art\\title.clx");
		if (!ref.ok()) {
			LogError("{}", SDL_GetError());
			InsertCDDlg(_("diabdat.mpq or spawn.mpq"));
		}
	}
	hellfire_data_path = FindUnpackedMpqData(paths, "hellfire");
	if (hellfire_data_path)
		gbIsHellfire = true;
	if (forceHellfire && !hellfire_data_path)
		InsertCDDlg("hellfire");

	const bool hasMonk = FileExists(*hellfire_data_path + "plrgfx/monk/mha/mhaas.clx");
	const bool hasMusic = FileExists(*hellfire_data_path + "music/dlvlf.wav")
	    || FileExists(*hellfire_data_path + "music/dlvlf.mp3");
	const bool hasVoice = FileExists(*hellfire_data_path + "sfx/hellfire/cowsut1.wav")
	    || FileExists(*hellfire_data_path + "sfx/hellfire/cowsut1.mp3");

	if (gbIsHellfire && (!hasMonk || !hasMusic || !hasVoice)) {
		DisplayFatalErrorAndExit(_("Some Hellfire MPQs are missing"), _("Not all Hellfire MPQs were found.\nPlease copy all the hf*.mpq files."));
	}
#else // !UNPACKED_MPQS
	diabdat_mpq = LoadMPQ(paths, "diabdat.mpq");

	if (!diabdat_mpq) {
		spawn_mpq = LoadMPQ(paths, "spawn.mpq");
		if (spawn_mpq)
			gbIsSpawn = true;
	}
	if (!HeadlessMode) {
		AssetRef ref = FindAsset("ui_art\\title.pcx");
		if (!ref.ok()) {
			LogError("{}", SDL_GetError());
			InsertCDDlg(_("diabdat.mpq or spawn.mpq"));
		}
	}

	hellfire_mpq = LoadMPQ(paths, "hellfire.mpq");
	if (hellfire_mpq)
		gbIsHellfire = true;
	if (forceHellfire && !hellfire_mpq)
		InsertCDDlg("hellfire.mpq");

	hfmonk_mpq = LoadMPQ(paths, "hfmonk.mpq");
	hfbard_mpq = LoadMPQ(paths, "hfbard.mpq");
	hfbarb_mpq = LoadMPQ(paths, "hfbarb.mpq");
	hfmusic_mpq = LoadMPQ(paths, "hfmusic.mpq");
	hfvoice_mpq = LoadMPQ(paths, "hfvoice.mpq");

	if (gbIsHellfire && (!hfmonk_mpq || !hfmusic_mpq || !hfvoice_mpq)) {
		DisplayFatalErrorAndExit(_("Some Hellfire MPQs are missing"), _("Not all Hellfire MPQs were found.\nPlease copy all the hf*.mpq files."));
	}
#endif
}

} // namespace devilution
