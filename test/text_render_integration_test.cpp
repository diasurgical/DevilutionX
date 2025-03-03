#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <variant>

#include <SDL.h>
#include <expected.hpp>
#include <function_ref.hpp>

#include "engine/load_file.hpp"
#include "engine/palette.h"
#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/size.hpp"
#include "engine/surface.hpp"
#include "utils/paths.h"
#include "utils/sdl_wrap.h"
#include "utils/str_cat.hpp"
#include "utils/surface_to_png.hpp"

// Invoke with --update_expected to update the expected files with actual results.
static bool UpdateExpected;

namespace devilution {
namespace {

constexpr char FixturesPath[] = "test/fixtures/text_render_integration_test/";

struct TestFixture {
	std::string name;
	int width;
	int height;
	std::string_view fmt;
	std::vector<DrawStringFormatArg> args {};
	TextRenderOptions opts { .flags = UiFlags::ColorUiGold };

	friend void PrintTo(const TestFixture &f, std::ostream *os)
	{
		*os << f.name;
	}
};

const TestFixture Fixtures[] {
	TestFixture {
	    .name = "basic",
	    .width = 96,
	    .height = 15,
	    .fmt = "DrawString",
	},
	TestFixture {
	    .name = "basic-colors",
	    .width = 186,
	    .height = 15,
	    .fmt = "{}{}{}{}",
	    .args = {
	        { "Draw", UiFlags::ColorUiSilver },
	        { "String", UiFlags::ColorUiGold },
	        { "With", UiFlags::ColorUiSilverDark },
	        { "Colors", UiFlags::ColorUiGoldDark },
	    },
	},
	TestFixture {
	    .name = "horizontal_overflow",
	    .width = 50,
	    .height = 28,
	    .fmt = "Horizontal",
	},
	TestFixture {
	    .name = "horizontal_overflow-colors",
	    .width = 50,
	    .height = 28,
	    .fmt = "{}{}",
	    .args = {
	        { "Hori", UiFlags::ColorUiGold },
	        { "zontal", UiFlags::ColorUiSilverDark },
	    },
	},
	TestFixture {
	    .name = "kerning_fit_spacing",
	    .width = 120,
	    .height = 15,
	    .fmt = "KerningFitSpacing",
	    .opts = {
	        .flags = UiFlags::KerningFitSpacing | UiFlags::ColorUiSilver,
	    },
	},
	TestFixture {
	    .name = "kerning_fit_spacing-colors",
	    .width = 120,
	    .height = 15,
	    .fmt = "{}{}{}",
	    .args = {
	        { "Kerning", UiFlags::ColorUiSilver },
	        { "Fit", UiFlags::ColorUiGold },
	        { "Spacing", UiFlags::ColorUiSilverDark },
	    },
	    .opts = {
	        .flags = UiFlags::KerningFitSpacing,
	    },
	},
	TestFixture {
	    .name = "kerning_fit_spacing__align_center",
	    .width = 170,
	    .height = 15,
	    .fmt = "KerningFitSpacing | AlignCenter",
	    .opts = {
	        .flags = UiFlags::KerningFitSpacing | UiFlags::AlignCenter | UiFlags::ColorUiSilver,
	    },
	},
	TestFixture {
	    .name = "kerning_fit_spacing__align_center-colors",
	    .width = 170,
	    .height = 15,
	    .fmt = "{}{}{}",
	    .args = {
	        { "KerningFitSpacing", UiFlags::ColorUiSilver },
	        { " | ", UiFlags::ColorUiGold },
	        { "AlignCenter", UiFlags::ColorUiSilverDark },
	    },
	    .opts = {
	        .flags = UiFlags::KerningFitSpacing | UiFlags::AlignCenter,
	    },
	},
	TestFixture {
	    .name = "kerning_fit_spacing__align_center__newlines",
	    .width = 170,
	    .height = 42,
	    .fmt = "KerningFitSpacing | AlignCenter\nShort line\nAnother overly long line",
	    .opts = {
	        .flags = UiFlags::KerningFitSpacing | UiFlags::AlignCenter | UiFlags::ColorUiSilver,
	    },
	},
	TestFixture {
	    .name = "kerning_fit_spacing__align_center__newlines_in_fmt-colors",
	    .width = 170,
	    .height = 42,
	    .fmt = "{}\n{}\n{}",
	    .args = {
	        { "KerningFitSpacing | AlignCenter", UiFlags::ColorUiSilver },
	        { "Short line", UiFlags::ColorUiGold },
	        { "Another overly long line", UiFlags::ColorUiSilverDark },
	    },
	    .opts = {
	        .flags = UiFlags::KerningFitSpacing | UiFlags::AlignCenter,
	    },
	},
	TestFixture {
	    .name = "kerning_fit_spacing__align_center__newlines_in_value-colors",
	    .width = 170,
	    .height = 42,
	    .fmt = "{}{}",
	    .args = {
	        { "KerningFitSpacing | AlignCenter\nShort line\nAnother overly ", UiFlags::ColorUiSilver },
	        { "long line", UiFlags::ColorUiGold },
	    },
	    .opts = {
	        .flags = UiFlags::KerningFitSpacing | UiFlags::AlignCenter,
	    },
	},
	TestFixture {
	    .name = "kerning_fit_spacing__align_right",
	    .width = 170,
	    .height = 15,
	    .fmt = "KerningFitSpacing | AlignRight",
	    .opts = {
	        .flags = UiFlags::KerningFitSpacing | UiFlags::AlignRight | UiFlags::ColorUiSilver,
	    },
	},
	TestFixture {
	    .name = "kerning_fit_spacing__align_right-colors",
	    .width = 170,
	    .height = 15,
	    .fmt = "{}{}{}",
	    .args = {
	        { "KerningFitSpacing", UiFlags::ColorUiSilver },
	        { " | ", UiFlags::ColorUiGold },
	        { "AlignRight", UiFlags::ColorUiSilverDark },
	    },
	    .opts = {
	        .flags = UiFlags::KerningFitSpacing | UiFlags::AlignRight,
	    },
	},
	TestFixture {
	    .name = "vertical_overflow",
	    .width = 36,
	    .height = 20,
	    .fmt = "One\nTwo",
	},
	TestFixture {
	    .name = "vertical_overflow-colors",
	    .width = 36,
	    .height = 20,
	    .fmt = "{}\n{}",
	    .args = {
	        { "One", UiFlags::ColorUiGold },
	        { "Two", UiFlags::ColorUiSilverDark },
	    },
	},
};

SDLPaletteUniquePtr LoadPalette()
{
	struct Color {
		uint8_t r, g, b;
	};
	std::array<Color, 256> palData;
	LoadFileInMem("ui_art\\diablo.pal", palData);
	SDLPaletteUniquePtr palette = SDLWrap::AllocPalette(static_cast<int>(palData.size()));
	for (unsigned i = 0; i < palData.size(); i++) {
		palette->colors[i] = SDL_Color {
			palData[i].r, palData[i].g, palData[i].b, SDL_ALPHA_OPAQUE
		};
	}
	return palette;
}

std::vector<std::byte> ReadFile(const std::string &path)
{
	SDL_RWops *rwops = SDL_RWFromFile(path.c_str(), "rb");
	std::vector<std::byte> result;
	if (rwops == nullptr) return result;
	const size_t size = SDL_RWsize(rwops);
	result.resize(size);
	SDL_RWread(rwops, result.data(), size, 1);
	SDL_RWclose(rwops);
	return result;
}

void DrawWithBorder(const Surface &out, const Rectangle &area, tl::function_ref<void(const Rectangle &)> fn)
{
	const uint8_t debugColor = PAL8_RED;
	DrawHorizontalLine(out, area.position, area.size.width, debugColor);
	DrawHorizontalLine(out, area.position + Displacement { 0, area.size.height - 1 }, area.size.width, debugColor);
	DrawVerticalLine(out, area.position, area.size.height, debugColor);
	DrawVerticalLine(out, area.position + Displacement { area.size.width - 1, 0 }, area.size.height, debugColor);
	fn(Rectangle {
	    Point { area.position.x + 1, area.position.y + 1 },
	    Size { area.size.width - 2, area.size.height - 2 } });
}

MATCHER_P(FileContentsEq, expectedPath,
    StrCat(negation ? "doesn't have" : "has", " the same contents as ", ::testing::PrintToString(expectedPath)))
{
	if (ReadFile(arg) != ReadFile(expectedPath)) {
		if (UpdateExpected) {
			CopyFileOverwrite(arg.c_str(), expectedPath.c_str());
			std::clog << "⬆️ Updated expected file at " << expectedPath << std::endl;
			return true;
		}
		return false;
	}
	return true;
}

class TextRenderIntegrationTest : public ::testing::TestWithParam<TestFixture> {
public:
	static void SetUpTestSuite()
	{
		palette = LoadPalette();
	}
	static void TearDownTestSuite()
	{
		palette = nullptr;
	}

protected:
	static SDLPaletteUniquePtr palette;
};

SDLPaletteUniquePtr TextRenderIntegrationTest::palette;

TEST_P(TextRenderIntegrationTest, RenderAndCompareTest)
{
	const TestFixture &fixture = GetParam();

	OwnedSurface out = OwnedSurface { fixture.width + 20, fixture.height + 20 };
	SDL_SetSurfacePalette(out.surface, palette.get());
	ASSERT_NE(out.surface, nullptr);

	DrawWithBorder(out, Rectangle { Point { 10, 10 }, Size { fixture.width, fixture.height } }, [&](const Rectangle &rect) {
		if (fixture.args.empty()) {
			DrawString(out, fixture.fmt, rect, fixture.opts);
		} else {
			DrawStringWithColors(out, fixture.fmt, fixture.args, rect, fixture.opts);
		}
	});

	const std::string actualPath = StrCat(paths::BasePath(), FixturesPath, GetParam().name, "-Actual.png");
	const std::string expectedPath = StrCat(paths::BasePath(), FixturesPath, GetParam().name, ".png");
	SDL_RWops *actual = SDL_RWFromFile(actualPath.c_str(), "wb");
	ASSERT_NE(actual, nullptr) << SDL_GetError();
	ASSERT_TRUE(WriteSurfaceToFilePng(out, actual).has_value());

	EXPECT_THAT(actualPath, FileContentsEq(expectedPath));
}

INSTANTIATE_TEST_SUITE_P(GoldenTests, TextRenderIntegrationTest,
    testing::ValuesIn(Fixtures),
    [](const testing::TestParamInfo<TestFixture> &info) {
	    std::string name = info.param.name;
	    std::replace(name.begin(), name.end(), '-', '_');
	    return name;
    });

} // namespace
} // namespace devilution

int main(int argc, char **argv)
{
	::testing::InitGoogleTest(&argc, argv);
	if (argc >= 2) {
		for (int i = 1; i < argc; ++i) {
			if (argv[i] != std::string_view("--update_expected")) {
				std::cerr << "unknown argument: " << argv[i] << "\nUsage: "
				          << argv[0] << " [--update_expected]" << "\n";
				return 64;
			}
		}
		UpdateExpected = true;
	}
	return RUN_ALL_TESTS();
}
