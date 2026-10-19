#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "options.h"
#include "utils/paths.h"

using namespace devilution;

namespace {

class ModDiscoveryTest : public ::testing::Test {
protected:
	void SetUp() override
	{
		savedPrefPath_ = paths::PrefPath();
		savedBasePath_ = paths::BasePath();
		savedConfigPath_ = paths::ConfigPath();
		const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
		root_ = std::filesystem::path("Test_ModDiscovery") / info->name();
		paths::SetPrefPath((root_ / "pref").string());
		paths::SetBasePath((root_ / "base").string());
		paths::SetConfigPath((root_ / "config").string());
	}

	void TearDown() override
	{
		paths::SetPrefPath(savedPrefPath_);
		paths::SetBasePath(savedBasePath_);
		paths::SetConfigPath(savedConfigPath_);
		std::filesystem::remove_all(root_);
	}

	void Write(const std::string &relative, const std::string &content = "")
	{
		const std::filesystem::path path = root_ / relative;
		std::filesystem::create_directories(path.parent_path());
		std::ofstream file(path, std::ios::binary);
		ASSERT_TRUE(file.is_open());
		file << content;
		ASSERT_TRUE(file.good());
	}

private:
	std::filesystem::path root_;
	std::string savedPrefPath_;
	std::string savedBasePath_;
	std::string savedConfigPath_;
};

TEST_F(ModDiscoveryTest, ManifestOnlyModRetainsEnabledIniSetting)
{
	Write("pref/mods/abysmal/manifest.ini", "[mod]\nname=Abysmal Diablo V2+\n");
	Write("config/diablo.ini", "[Mods]\nabysmal=1\n");

	LoadOptions();

	const auto active = GetOptions().Mods.GetActiveModList();
	EXPECT_NE(std::find(active.begin(), active.end(), "abysmal"), active.end());
	const auto entries = GetOptions().Mods.GetEntries();
	EXPECT_NE(std::find_if(entries.begin(), entries.end(), [](const OptionEntryBase *entry) {
		return entry->GetName() == "Abysmal Diablo V2+";
	}), entries.end());
}

TEST_F(ModDiscoveryTest, FindsExecutableModsAndLegacyLuaModsWithoutDuplicates)
{
	Write("base/mods/base_data/manifest.ini", "[mod]\nname=Base Data\n");
	Write("pref/mods/legacy/lua/mods/legacy/init.lua", "return {}\n");
	Write("base/mods/legacy/lua/mods/legacy/init.lua", "return {}\n");
	Write("pref/mods/packed.mpq");
	Write("pref/mods/ordinary/readme.txt");
	std::filesystem::create_directories(std::filesystem::path(paths::PrefPath()) / "mods" / "empty");
	Write("config/diablo.ini", "[Mods]\nbase_data=1\nlegacy=1\nordinary=1\n");

	LoadOptions();

	const auto mods = GetOptions().Mods.GetModList();
	EXPECT_EQ(std::count(mods.begin(), mods.end(), "base_data"), 1);
	EXPECT_EQ(std::count(mods.begin(), mods.end(), "legacy"), 1);
	EXPECT_EQ(std::count(mods.begin(), mods.end(), "packed"), 1);
	EXPECT_EQ(std::count(mods.begin(), mods.end(), "ordinary"), 0);
	EXPECT_EQ(std::count(mods.begin(), mods.end(), "empty"), 0);
	const auto active = GetOptions().Mods.GetActiveModList();
	EXPECT_NE(std::find(active.begin(), active.end(), "base_data"), active.end());
	EXPECT_NE(std::find(active.begin(), active.end(), "legacy"), active.end());
}

} // namespace
