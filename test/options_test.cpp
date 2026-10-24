// Written by VTSTech (Nigel Todman) (veritas@vts-tech.org)
// AI Assisted by Super-Z (GLM-5.3-Flash)
// License: MIT

/**
 * Options-layer regression tests (TEST-03): the ini round-trip and the
 * shader-filter self-heal logic the fork added to options.cpp, plus the
 * corrupt-ini recovery path (ROB-09).
 *
 * Headless: no GPU, no window, no SDL video/audio. Two location seams:
 *  - diablo.ini lives under a per-process temp dir (paths::SetConfigPath),
 *    so parallel ctest processes (gtest_discover_tests runs each case in
 *    its own process) never share state;
 *  - the runtime shaders root is SDL_GetBasePath() + "shaders", which for a
 *    test binary is the executable's directory (the build dir). A fixed set
 *    of preset fixtures is staged there idempotently (same content every
 *    run, so concurrent processes writing them is harmless) and left in
 *    place for the next run.
 */
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

#include <gtest/gtest.h>

#include <SDL3/SDL_filesystem.h>

#include "options.h"
#include "utils/paths.h"

// The options machinery lives in devilution::; open it for the test cases.
using namespace devilution;

namespace {

long ProcessId()
{
#if defined(_WIN32)
	return static_cast<long>(::_getpid());
#else
	return static_cast<long>(::getpid());
#endif
}

const std::string kWorkRoot = (std::filesystem::temp_directory_path() / ("devilutionx_options_test_" + std::to_string(ProcessId())))
                                  .string();
// Fixed relative location inside the shader root, so staging stays idempotent
// across processes and runs. Preset paths in the ini are relative to the root.
const std::string kStagedPrefix = "options_test_fixtures";
const std::string kPresetPath = kStagedPrefix + "/packs/test.slangp";

std::string gShaderRoot; // SDL_GetBasePath() + "shaders", set in SetUpTestSuite
std::string gStagedDir;  // gShaderRoot + "/" + kStagedPrefix

void WriteFileOrDie(const std::string &path, const std::string &content)
{
	std::error_code ec;
	std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
	std::ofstream out(path, std::ios::binary | std::ios::trunc);
	if (!out.good()) {
		ADD_FAILURE() << "cannot write " << path;
		return;
	}
	out << content;
}

/** Stages a minimal browsable shader root: <staged>/packs/test.slangp plus
 *  the shader file its chain names. Only the listing matters here — no preset
 *  is parsed or loaded (no GPU anywhere in this binary). */
void StageShaderRoot()
{
	WriteFileOrDie(gStagedDir + "/packs/test.slangp", "shader0 = simple.slang\n");
	WriteFileOrDie(gStagedDir + "/packs/simple.slang",
	    "#pragma name simple\nvoid main() { }\n");
}

class OptionsShaderFilterTest : public ::testing::Test {
public:
	static void SetUpTestSuite()
	{
		const char *basePath = SDL_GetBasePath();
		const std::string exeDir = (basePath != nullptr) ? basePath : std::string("./");
		gShaderRoot = exeDir + "shaders";
		gStagedDir = gShaderRoot + "/" + kStagedPrefix;
		paths::SetPrefPath(kWorkRoot);
		paths::SetConfigPath(kWorkRoot);
	}

protected:
	void SetUp() override
	{
		std::error_code ec;
		std::filesystem::remove_all(kWorkRoot, ec);
		std::filesystem::create_directories(kWorkRoot, ec);
		StageShaderRoot();
	}

	static int FilterValue()
	{
		return static_cast<int>(*GetOptions().Graphics.shaderFilter);
	}

	static const std::string &PresetPath()
	{
		return GetOptions().Graphics.shaderFilterPreset.value;
	}

	void LoadIniText(const std::string &text)
	{
		WriteFileOrDie(kWorkRoot + "/diablo.ini", text);
		LoadOptions();
	}
};

TEST_F(OptionsShaderFilterTest, StaleBuiltinValuesSanitizeToOff)
{
	// Builtin HQ2x/ScaleFX filters were removed; ini values from that range
	// (1..kShaderFilterCustomBase-1) must fall back to Off, not activate an
	// arbitrary filter.
	LoadIniText("[Graphics]\nShader Filter=5\n");
	EXPECT_EQ(FilterValue(), 0);
	EXPECT_EQ(static_cast<int>(ShaderFilter::None), 0);
	EXPECT_TRUE(PresetPath().empty());
}

TEST_F(OptionsShaderFilterTest, StaleBuiltinEdgeValuesSanitizeToOff)
{
	LoadIniText("[Graphics]\nShader Filter=1\n");
	EXPECT_EQ(FilterValue(), 0);
	// 99 = kShaderFilterCustomBase - 1: the last stale builtin value.
	LoadIniText("[Graphics]\nShader Filter=99\n");
	EXPECT_EQ(FilterValue(), 0);
	// Off (0) itself is preserved as-is.
	LoadIniText("[Graphics]\nShader Filter=0\n");
	EXPECT_EQ(FilterValue(), 0);
}

TEST_F(OptionsShaderFilterTest, CustomIndexMapsToStoredPresetPath)
{
	// Value 100+N is an index into the browsed folder's listing; on load it is
	// re-resolved from the persisted preset path so a stale index cannot point
	// at an unrelated preset after the folder contents change.
	LoadIniText("[Graphics]\nShader Filter=100\nShader Filter Preset=" + kPresetPath + "\n");
	EXPECT_EQ(FilterValue(), 100); // first (only) preset in the scan
	EXPECT_EQ(PresetPath(), kPresetPath);
}

TEST_F(OptionsShaderFilterTest, MissingStoredPresetSelfHealsToOff)
{
	// The persisted preset no longer exists: the custom value must reset to
	// Off AND the stale path must be cleared, so the stale value cannot point
	// at an unrelated preset.
	LoadIniText("[Graphics]\nShader Filter=100\nShader Filter Preset=" + kStagedPrefix + "/packs/gone.slangp\n");
	EXPECT_EQ(FilterValue(), 0);
	EXPECT_TRUE(PresetPath().empty());
}

TEST_F(OptionsShaderFilterTest, IniRoundTripPreservesBothKeys)
{
	// Start from defaults, select the preset the way the settings browser
	// would (rescan resolves the value from the stored path), then save.
	LoadOptions();
	GetOptions().Graphics.shaderFilterPreset.value = kPresetPath;
	GetOptions().Graphics.shaderFilter.RescanCustomEntries();
	ASSERT_EQ(FilterValue(), 100);
	SaveOptions();

	// Simulate a restart: reload from the file on disk and check both keys.
	LoadOptions();
	EXPECT_EQ(FilterValue(), 100);
	EXPECT_EQ(PresetPath(), kPresetPath);
}

TEST_F(OptionsShaderFilterTest, CorruptIniRecoversInsteadOfAborting)
{
	// ROB-09: an unclosed section fails Ini::parse. Startup must NOT abort
	// (the pre-fix app_fatal), the corrupt file must be preserved for
	// debugging under a .corrupt-<timestamp> sibling, and defaults load.
	WriteFileOrDie(kWorkRoot + "/diablo.ini", "[Graphics\nShader Filter=5\n");
	LoadOptions();
	EXPECT_EQ(FilterValue(), 0);

	std::error_code ec;
	bool foundCorruptCopy = false;
	for (const std::filesystem::directory_entry &entry :
	    std::filesystem::directory_iterator(kWorkRoot, ec)) {
		const std::string name = entry.path().filename().string();
		if (name.rfind("diablo.ini.corrupt-", 0) == 0) {
			foundCorruptCopy = true;
			std::ifstream in(entry.path(), std::ios::binary);
			std::string preserved((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			EXPECT_NE(preserved.find("[Graphics"), std::string::npos)
			    << "the preserved copy must contain the corrupt content";
		}
	}
	EXPECT_TRUE(foundCorruptCopy) << "the corrupt ini was not preserved";
	EXPECT_FALSE(std::filesystem::exists(kWorkRoot + "/diablo.ini", ec))
	    << "the corrupt file must be moved aside so the next save writes clean";
}

TEST_F(OptionsShaderFilterTest, SaveAfterCorruptIniWritesCleanFile)
{
	// The recovery contract: defaults load, the next SaveIni writes a clean
	// file, and a subsequent load reads it back without errors.
	WriteFileOrDie(kWorkRoot + "/diablo.ini", "[Graphics\nShader Filter=5\n");
	LoadOptions();
	GetOptions().Graphics.shaderFilterPreset.value = kPresetPath;
	GetOptions().Graphics.shaderFilter.RescanCustomEntries();
	SaveOptions();

	LoadOptions();
	EXPECT_EQ(FilterValue(), 100);
	EXPECT_EQ(PresetPath(), kPresetPath);
	EXPECT_TRUE(std::filesystem::exists(kWorkRoot + "/diablo.ini"));
}

} // namespace
