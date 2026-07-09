#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <picosha2.h>

#include "mods/mod_identity.h"

using namespace devilution;

namespace {

std::string TmpPath(const char *suffix)
{
	const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
	return std::string("Test_") + info->test_case_name() + '_' + info->name() + suffix;
}

void WriteFile(const std::string &path, const void *data, size_t size)
{
	FILE *file = std::fopen(path.c_str(), "wb");
	ASSERT_TRUE(file != nullptr);
	if (size > 0) {
		ASSERT_EQ(std::fwrite(data, size, 1, file), 1u);
	}
	std::fclose(file);
}

TEST(ModIdentity, HashesKnownVectors)
{
	// SHA-256("abc")
	const std::string abcPath = TmpPath("_abc");
	WriteFile(abcPath, "abc", 3);
	std::array<uint8_t, 32> hash;
	ASSERT_TRUE(ComputeFileSha256(abcPath.c_str(), hash));
	EXPECT_EQ(ModHashToHex(hash), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

	// SHA-256("") — empty file.
	const std::string emptyPath = TmpPath("_empty");
	WriteFile(emptyPath, "", 0);
	ASSERT_TRUE(ComputeFileSha256(emptyPath.c_str(), hash));
	EXPECT_EQ(ModHashToHex(hash), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(ModIdentity, ChunkedReadMatchesOneShot)
{
	// Larger than the 32 KiB internal read buffer, to exercise the chunking loop and
	// verify it agrees with a single in-memory hash of the same bytes.
	std::vector<uint8_t> data(100000);
	for (size_t i = 0; i < data.size(); ++i)
		data[i] = static_cast<uint8_t>((i * 31 + 7) & 0xFF);

	const std::string path = TmpPath("_big");
	WriteFile(path, data.data(), data.size());

	std::array<uint8_t, 32> fileHash;
	ASSERT_TRUE(ComputeFileSha256(path.c_str(), fileHash));

	std::array<uint8_t, 32> memHash;
	picosha2::hash256(data.begin(), data.end(), memHash.begin(), memHash.end());

	EXPECT_EQ(fileHash, memHash);
}

TEST(ModIdentity, MissingFileFails)
{
	std::array<uint8_t, 32> hash;
	EXPECT_FALSE(ComputeFileSha256("this-file-should-not-exist.mpq", hash));
}

TEST(ModIdentity, EmptyWhitelistRejectsEverything)
{
	std::array<uint8_t, 32> hash = {};
	EXPECT_FALSE(IsHashWhitelisted(hash));
	hash[0] = 0xAB;
	EXPECT_FALSE(IsHashWhitelisted(hash));
}

TEST(ModIdentity, HexToModHashRoundTrip)
{
	std::array<uint8_t, 32> hash;
	for (size_t i = 0; i < hash.size(); ++i)
		hash[i] = static_cast<uint8_t>(i * 7 + 1);
	const std::string hex = ModHashToHex(hash);

	std::array<uint8_t, 32> decoded;
	ASSERT_TRUE(HexToModHash(hex, decoded));
	EXPECT_EQ(decoded, hash);

	// Uppercase is accepted too.
	std::string upper = hex;
	for (char &c : upper)
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	ASSERT_TRUE(HexToModHash(upper, decoded));
	EXPECT_EQ(decoded, hash);
}

TEST(ModIdentity, HexToModHashRejectsMalformed)
{
	std::array<uint8_t, 32> decoded;
	EXPECT_FALSE(HexToModHash("", decoded));                   // too short
	EXPECT_FALSE(HexToModHash(std::string(63, 'a'), decoded)); // 63 digits
	EXPECT_FALSE(HexToModHash(std::string(65, 'a'), decoded)); // 65 digits
	EXPECT_FALSE(HexToModHash(std::string(64, 'g'), decoded)); // non-hex digit
}

TEST(ModIdentity, ParsesManifestFields)
{
	const std::string_view manifest = "[mod]\n"
	                                  "name=Viking Total Conversion\n"
	                                  "description=A total conversion mod\n"
	                                  "version=2.1.0\n"
	                                  "author=Someone\n"
	                                  "homepage=https://example.com\n"
	                                  "license=GPL-3.0\n"
	                                  "saveExtension=.vsv\n"
	                                  "programId=DXVK\n"
	                                  "requires=clock\n"
	                                  "requires=adria_refills_mana\n"
	                                  "compatible=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n"
	                                  "compatible=e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855\n";

	const ModManifest parsed = ParseModManifest(manifest);
	EXPECT_EQ(parsed.name, "Viking Total Conversion");
	EXPECT_EQ(parsed.description, "A total conversion mod");
	EXPECT_EQ(parsed.version, "2.1.0");
	EXPECT_EQ(parsed.author, "Someone");
	EXPECT_EQ(parsed.homepage, "https://example.com");
	EXPECT_EQ(parsed.license, "GPL-3.0");
	EXPECT_EQ(parsed.saveExtension, ".vsv");
	EXPECT_EQ(parsed.programId, "DXVK");
	ASSERT_EQ(parsed.requiredMods.size(), 2u);
	EXPECT_EQ(parsed.requiredMods[0], "clock");
	EXPECT_EQ(parsed.requiredMods[1], "adria_refills_mana");
	ASSERT_EQ(parsed.compatibleWith.size(), 2u);
	EXPECT_EQ(ModHashToHex(parsed.compatibleWith[0]), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
	EXPECT_EQ(ModHashToHex(parsed.compatibleWith[1]), "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(ModIdentity, ManifestIgnoresInvalidCompatibleAndMissingFields)
{
	const std::string_view manifest = "[mod]\n"
	                                  "name=Minimal\n"
	                                  "compatible=not-a-hash\n"
	                                  "compatible=ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\n";

	const ModManifest parsed = ParseModManifest(manifest);
	EXPECT_EQ(parsed.name, "Minimal");
	EXPECT_TRUE(parsed.version.empty());
	EXPECT_TRUE(parsed.requiredMods.empty());
	// Only the valid identifier is kept.
	ASSERT_EQ(parsed.compatibleWith.size(), 1u);
	EXPECT_EQ(ModHashToHex(parsed.compatibleWith[0]), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(ModIdentity, EmptyManifestYieldsEmptyFields)
{
	const ModManifest parsed = ParseModManifest("");
	EXPECT_TRUE(parsed.name.empty());
	EXPECT_TRUE(parsed.compatibleWith.empty());
	EXPECT_TRUE(parsed.requiredMods.empty());
}

TEST(ModIdentity, SatisfiesRequiredIdentifier)
{
	ModIdentifier mod;
	mod.name = "example";
	ASSERT_TRUE(HexToModHash("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", mod.hash));

	std::array<uint8_t, 32> compat;
	ASSERT_TRUE(HexToModHash("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", compat));
	mod.manifest.compatibleWith.push_back(compat);

	// Exact hash match.
	EXPECT_TRUE(SatisfiesRequiredIdentifier(mod, mod.hash));
	// Listed in the compat list.
	EXPECT_TRUE(SatisfiesRequiredIdentifier(mod, compat));
	// Neither.
	std::array<uint8_t, 32> other = {};
	other[0] = 0x01;
	EXPECT_FALSE(SatisfiesRequiredIdentifier(mod, other));
}

TEST(ModIdentity, RegisterAndClear)
{
	ClearModIdentifiers();
	ASSERT_TRUE(ActiveModIdentifiers.empty());

	const std::string path = TmpPath("_mod");
	WriteFile(path, "abc", 3);

	RegisterBuiltinModIdentifier("clock");
	RegisterPackedModIdentifier("external", path.c_str());

	ASSERT_EQ(ActiveModIdentifiers.size(), 2u);

	// Built-in: provenance-whitelisted, zero hash.
	EXPECT_EQ(ActiveModIdentifiers[0].name, "clock");
	EXPECT_TRUE(ActiveModIdentifiers[0].whitelisted);
	EXPECT_EQ(ActiveModIdentifiers[0].hash, (std::array<uint8_t, 32> {}));

	// Packed: hashed, not whitelisted (empty approved list).
	EXPECT_EQ(ActiveModIdentifiers[1].name, "external");
	EXPECT_FALSE(ActiveModIdentifiers[1].whitelisted);
	EXPECT_EQ(ModHashToHex(ActiveModIdentifiers[1].hash), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

	ClearModIdentifiers();
	EXPECT_TRUE(ActiveModIdentifiers.empty());
}

TEST(ModIdentity, ActiveModSaveExtensionOptInLastWins)
{
	ClearModIdentifiers();
	// No active mods: no override (callers default to "sv").
	EXPECT_TRUE(GetActiveModSaveExtension().empty());

	// A cosmetic mod that declares no saveExtension leaves the namespace untouched.
	ModIdentifier cosmetic;
	cosmetic.name = "clock";
	ActiveModIdentifiers.push_back(cosmetic);
	EXPECT_TRUE(GetActiveModSaveExtension().empty());

	// A mod that opts in overrides it; a leading dot is stripped.
	ModIdentifier hf;
	hf.name = "hf";
	hf.manifest.saveExtension = ".hsv";
	ActiveModIdentifiers.push_back(hf);
	EXPECT_EQ(GetActiveModSaveExtension(), "hsv");

	// The last active mod that declares one wins.
	ModIdentifier other;
	other.name = "other";
	other.manifest.saveExtension = "msv";
	ActiveModIdentifiers.push_back(other);
	EXPECT_EQ(GetActiveModSaveExtension(), "msv");

	ClearModIdentifiers();
}

TEST(ModIdentity, OrderModsDependenciesFirst)
{
	const std::vector<ModDependency> mods = {
		{ "app", { "lib" } },
		{ "lib", {} },
	};
	const std::vector<std::string> ordered = OrderModsByDependencies(mods);
	ASSERT_EQ(ordered.size(), 2u);
	EXPECT_EQ(ordered[0], "lib");
	EXPECT_EQ(ordered[1], "app");
}

TEST(ModIdentity, OrderModsStableWithoutDependencies)
{
	const std::vector<ModDependency> mods = { { "a", {} }, { "b", {} }, { "c", {} } };
	const std::vector<std::string> ordered = OrderModsByDependencies(mods);
	EXPECT_EQ(ordered, (std::vector<std::string> { "a", "b", "c" }));
}

TEST(ModIdentity, OrderModsTransitiveChain)
{
	const std::vector<ModDependency> mods = {
		{ "c", { "b" } },
		{ "b", { "a" } },
		{ "a", {} },
	};
	const std::vector<std::string> ordered = OrderModsByDependencies(mods);
	ASSERT_EQ(ordered.size(), 3u);
	EXPECT_EQ(ordered[0], "a");
	EXPECT_EQ(ordered[1], "b");
	EXPECT_EQ(ordered[2], "c");
}

TEST(ModIdentity, OrderModsMissingDependencyKeepsMod)
{
	// The required mod is not part of the active set: "app" is still emitted (best-effort).
	const std::vector<ModDependency> mods = { { "app", { "missing" } } };
	const std::vector<std::string> ordered = OrderModsByDependencies(mods);
	ASSERT_EQ(ordered.size(), 1u);
	EXPECT_EQ(ordered[0], "app");
}

TEST(ModIdentity, OrderModsCycleKeepsAllMods)
{
	const std::vector<ModDependency> mods = { { "a", { "b" } }, { "b", { "a" } } };
	const std::vector<std::string> ordered = OrderModsByDependencies(mods);
	ASSERT_EQ(ordered.size(), 2u);
	EXPECT_NE(std::find(ordered.begin(), ordered.end(), "a"), ordered.end());
	EXPECT_NE(std::find(ordered.begin(), ordered.end(), "b"), ordered.end());
}

} // namespace
