#include "floatingnumbers.h"

#include <cstdint>
#include <ctime>
#include <deque>
#include <fmt/format.h>
#include <string>

#ifdef USE_SDL3
#include <SDL3/SDL_timer.h>
#else
#include <SDL.h>
#endif

#include "engine/render/text_render.hpp"
#include "options.h"
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

struct FloatingNumber {
	Point startPos;
	Displacement startOffset;
	Displacement endOffset;
	std::string text;
	uint32_t time;
	uint32_t lastMerge;
	UiFlags style;
	int id;
	bool reverseDirection;
};

std::deque<FloatingNumber> FloatingQueue;

void ClearExpiredNumbers()
{
	while (!FloatingQueue.empty()) {
		const FloatingNumber &num = FloatingQueue.front();
		if (num.time > SDL_GetTicks())
			break;

		FloatingQueue.pop_front();
	}
}

GameFontTables GetGameFontSize(UiFlags flags)
{
	if (HasAnyOf(flags, UiFlags::FontSize30))
		return GameFont30;
	if (HasAnyOf(flags, UiFlags::FontSize24))
		return GameFont24;
	return GameFont12;
}

} // namespace

void AddFloatingNumber(Point pos, Displacement offset, std::string text, UiFlags style, int id, bool reverseDirection)
{
	Displacement endOffset;
	if (!reverseDirection)
		endOffset = { 0, -140 };
	else
		endOffset = { 0, 140 };

	for (auto &num : FloatingQueue) {
		if (id != 0 && num.id == id && (SDL_GetTicks() - static_cast<int>(num.lastMerge)) <= 100) {
			num.text = text;
			num.lastMerge = SDL_GetTicks();
			num.style = style;
			num.startPos = pos;
			return;
		}
	}
	FloatingNumber num {
		pos, offset, endOffset, text,
		static_cast<uint32_t>(SDL_GetTicks() + 2500),
		static_cast<uint32_t>(SDL_GetTicks()),
		style | UiFlags::Outlined, id, reverseDirection
	};
	FloatingQueue.push_back(num);
}

void DrawFloatingNumbers(const Surface &out, Point viewPosition, Displacement offset)
{
	for (auto &floatingNum : FloatingQueue) {
		Displacement worldOffset = viewPosition - floatingNum.startPos;
		worldOffset = worldOffset.worldToScreen() + offset + Displacement { TILE_WIDTH / 2, -TILE_HEIGHT / 2 } + floatingNum.startOffset;

		if (*GetOptions().Graphics.zoom) {
			worldOffset *= 2;
		}

		Point screenPosition { worldOffset.deltaX, worldOffset.deltaY };

		const int lineWidth = GetLineWidth(floatingNum.text, GetGameFontSize(floatingNum.style));
		screenPosition.x -= lineWidth / 2;
		const uint32_t timeLeft = floatingNum.time - SDL_GetTicks();
		const float mul = 1 - (timeLeft / 2500.0f);
		screenPosition += floatingNum.endOffset * mul;

		DrawString(out, floatingNum.text, Rectangle { screenPosition, { lineWidth, 0 } },
		    { .flags = floatingNum.style });
	}

	ClearExpiredNumbers();
}

void ClearFloatingNumbers()
{
	srand(static_cast<unsigned int>(time(nullptr)));

	FloatingQueue.clear();
}

} // namespace devilution
