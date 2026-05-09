#include <gtest/gtest.h>

#include <iostream>
#include <string>
#include <unordered_map>

#include "headless_mode.hpp"
#include "options.h"
#include "utils/paths.h"

namespace {

// Custom listener to track and report skipped tests with reasons
class SkippedTestListener : public testing::EmptyTestEventListener {
	std::unordered_map<std::string, int> skipReasons;
	int totalSkipped = 0;

	void OnTestPartResult(const testing::TestPartResult &test_part_result) override
	{
		if (test_part_result.skipped()) {
			totalSkipped++;
			std::string reason = test_part_result.message();
			if (reason.empty()) {
				reason = "No reason provided";
			}
			skipReasons[reason]++;
		}
	}

	void OnTestProgramEnd(const testing::UnitTest & /*unit_test*/) override
	{
		if (totalSkipped > 0) {
			std::cout << "\n";
			std::cout << "========================================\n";
			std::cout << "Test Skip Summary\n";
			std::cout << "========================================\n";
			std::cout << "Total tests skipped: " << totalSkipped << "\n\n";

			// Show skip reasons, with most specific reasons first
			bool hasMpqReason = false;
			bool hasNoReason = false;
			int mpqSkipCount = 0;
			int noReasonCount = 0;

			for (const auto &[reason, count] : skipReasons) {
				if (reason.find("MPQ assets") != std::string::npos) {
					hasMpqReason = true;
					mpqSkipCount += count;
					continue;
				}
				if (reason == "No reason provided") {
					hasNoReason = true;
					noReasonCount += count;
					continue;
				}
				std::cout << "  • " << count << " test" << (count > 1 ? "s" : "") << " skipped: " << reason << "\n";
			}

			// Combine MPQ-related skips for clearer output
			if (hasMpqReason) {
				int totalMpqRelated = mpqSkipCount + (hasNoReason ? noReasonCount : 0);
				std::cout << "  • " << totalMpqRelated << " test" << (totalMpqRelated > 1 ? "s" : "")
				          << " skipped: MPQ assets (spawn.mpq or DIABDAT.MPQ) not found\n";
				if (hasNoReason && noReasonCount > 0) {
					std::cout << "    (" << noReasonCount << " test" << (noReasonCount > 1 ? "s" : "")
					          << " automatically skipped due to test suite setup failure)\n";
				}
			} else if (hasNoReason) {
				// Only "No reason provided" - show it as-is
				std::cout << "  • " << noReasonCount << " test" << (noReasonCount > 1 ? "s" : "")
				          << " skipped: " << "No reason provided" << "\n";
			}

			std::cout << "========================================\n";
		}
	}
};

} // namespace

int main(int argc, char **argv)
{
	// Disable error dialogs.
	devilution::HeadlessMode = true;

#if SDL_VERSION_ATLEAST(2, 0, 0)
	// Disable hardware cursor while testing.
	devilution::GetOptions().Graphics.hardwareCursor.SetValue(false);
#endif

#ifdef __APPLE__
	devilution::paths::SetAssetsPath(
	    devilution::paths::BasePath() + "devilutionx.app/Contents/Resources/");
#endif

	testing::InitGoogleTest(&argc, argv);

	// Add custom listener to track and report skipped tests
	testing::TestEventListeners &listeners = testing::UnitTest::GetInstance()->listeners();
	listeners.Append(new SkippedTestListener());

	return RUN_ALL_TESTS();
}
