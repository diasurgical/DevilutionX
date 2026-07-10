#include <gtest/gtest.h>

#include "control/control.hpp"
#include "diablo.h"
#include "engine/render/scrollrt.h"
#include "options.h"
#include "utils/ui_fwd.h"

using namespace devilution;

// TilesInView

TEST(Scroll_rt, calc_tiles_in_view_original)
{
	gnScreenWidth = 640;
	gnScreenHeight = 480;
	gnViewportHeight = gnScreenHeight - 128;
	GetOptions().Graphics.zoom.SetValue(false);
	int columns = 0;
	int rows = 0;
	TilesInView(&columns, &rows);
	EXPECT_EQ(columns, 10);
	EXPECT_EQ(rows, 11);
}

TEST(Scroll_rt, calc_tiles_in_view_original_zoom)
{
	gnScreenWidth = 640;
	gnScreenHeight = 480;
	gnViewportHeight = gnScreenHeight - 128;
	GetOptions().Graphics.zoom.SetValue(true);
	int columns = 0;
	int rows = 0;
	TilesInView(&columns, &rows);
	EXPECT_EQ(columns, 5);
	EXPECT_EQ(rows, 6);
}

TEST(Scroll_rt, calc_tiles_in_view_960_540)
{
	gnScreenWidth = 960;
	gnScreenHeight = 540;
	gnViewportHeight = gnScreenHeight;
	GetOptions().Graphics.zoom.SetValue(false);
	int columns = 0;
	int rows = 0;
	TilesInView(&columns, &rows);
	EXPECT_EQ(columns, 15);
	EXPECT_EQ(rows, 17);
}

TEST(Scroll_rt, calc_tiles_in_view_640_512)
{
	gnScreenWidth = 640;
	gnScreenHeight = 512;
	gnViewportHeight = gnScreenHeight - 128;
	GetOptions().Graphics.zoom.SetValue(false);
	int columns = 0;
	int rows = 0;
	TilesInView(&columns, &rows);
	EXPECT_EQ(columns, 10);
	EXPECT_EQ(rows, 12);
}

TEST(Scroll_rt, calc_tiles_in_view_768_480_zoom)
{
	gnScreenWidth = 768;
	gnScreenHeight = 480;
	gnViewportHeight = gnScreenHeight;
	GetOptions().Graphics.zoom.SetValue(true);
	int columns = 0;
	int rows = 0;
	TilesInView(&columns, &rows);
	EXPECT_EQ(columns, 6);
	EXPECT_EQ(rows, 8);
}

// CalcTileOffset

TEST(Scroll_rt, calc_tile_offset_original)
{
	gnScreenWidth = 640;
	gnScreenHeight = 480;
	gnViewportHeight = gnScreenHeight - 128;
	GetOptions().Graphics.zoom.SetValue(false);
	int x = 0;
	int y = 0;
	CalcTileOffset(&x, &y);
	EXPECT_EQ(x, 0);
	EXPECT_EQ(y, 0);
}

TEST(Scroll_rt, calc_tile_offset_original_zoom)
{
	gnScreenWidth = 640;
	gnScreenHeight = 480;
	gnViewportHeight = gnScreenHeight - 128;
	GetOptions().Graphics.zoom.SetValue(true);
	int x = 0;
	int y = 0;
	CalcTileOffset(&x, &y);
	EXPECT_EQ(x, 0);
	EXPECT_EQ(y, 8);
}

TEST(Scroll_rt, calc_tile_offset_960_540)
{
	gnScreenWidth = 960;
	gnScreenHeight = 540;
	gnViewportHeight = gnScreenHeight;
	GetOptions().Graphics.zoom.SetValue(false);
	int x = 0;
	int y = 0;
	CalcTileOffset(&x, &y);
	EXPECT_EQ(x, 0);
	EXPECT_EQ(y, 2);
}

TEST(Scroll_rt, calc_tile_offset_853_480)
{
	gnScreenWidth = 853;
	gnScreenHeight = 480;
	gnViewportHeight = gnScreenHeight;
	GetOptions().Graphics.zoom.SetValue(false);
	int x = 0;
	int y = 0;
	CalcTileOffset(&x, &y);
	EXPECT_EQ(x, 21);
	EXPECT_EQ(y, 0);
}

TEST(Scroll_rt, calc_tile_offset_768_480_zoom)
{
	gnScreenWidth = 768;
	gnScreenHeight = 480;
	gnViewportHeight = gnScreenHeight;
	GetOptions().Graphics.zoom.SetValue(true);
	int x = 0;
	int y = 0;
	CalcTileOffset(&x, &y);
	EXPECT_EQ(x, 0);
	EXPECT_EQ(y, 8);
}

// RowsCoveredByPanel

TEST(Scroll_rt, calc_tiles_covered_by_panel_original)
{
	gnScreenWidth = 640;
	GetOptions().Graphics.zoom.SetValue(false);
	CalculatePanelAreas();
	EXPECT_EQ(RowsCoveredByPanel(), 0);
}

TEST(Scroll_rt, calc_tiles_covered_by_panel_960)
{
	gnScreenWidth = 960;
	GetOptions().Graphics.zoom.SetValue(false);
	CalculatePanelAreas();
	EXPECT_EQ(RowsCoveredByPanel(), 4);
}

TEST(Scroll_rt, calc_tiles_covered_by_panel_960_zoom)
{
	gnScreenWidth = 960;
	GetOptions().Graphics.zoom.SetValue(true);
	CalculatePanelAreas();
	EXPECT_EQ(RowsCoveredByPanel(), 2);
}
