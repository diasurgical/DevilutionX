#include "engine/surface.hpp"

#include <cstdint>
#include <cstring>

#include "engine/render/renderer.h"

namespace devilution {

namespace {

template <bool SkipColorIndexZero>
void SurfaceBlit(const Surface &src, SDL_Rect srcRect, const Surface &dst, Point dstPosition)
{
	// We do not use `SDL_BlitSurface` here because the palettes may be different objects
	// and SDL would attempt to map them.

	dst.Clip(&srcRect, &dstPosition);
	if (srcRect.w <= 0 || srcRect.h <= 0)
		return;

	const std::uint8_t *srcBuf = src.at(srcRect.x, srcRect.y);
	const auto srcPitch = src.pitch();
	std::uint8_t *dstBuf = &dst[dstPosition];
	const auto dstPitch = dst.pitch();

	for (unsigned h = srcRect.h; h != 0; --h) {
		if (SkipColorIndexZero) {
			for (unsigned w = srcRect.w; w != 0; --w) {
				if (*srcBuf != 0)
					*dstBuf = *srcBuf;
				++srcBuf, ++dstBuf;
			}
			srcBuf += srcPitch - srcRect.w;
			dstBuf += dstPitch - srcRect.w;
		} else {
			std::memcpy(dstBuf, srcBuf, srcRect.w);
			srcBuf += srcPitch;
			dstBuf += dstPitch;
		}
	}
}

} // namespace

void Surface::BlitFrom(const Surface &src, SDL_Rect srcRect, Point targetPosition) const
{
	if (GetRenderer().NeedsFullRedraw() && isScreenTarget) {
		auto &renderer = GetRenderer();
		renderer.SetClipRegion(region.x, region.y, region.w, region.h);
		renderer.BlitSurface(src, srcRect, { targetPosition.x + region.x, targetPosition.y + region.y });
		renderer.ClearClipRegion();
		return;
	}
	SurfaceBlit</*SkipColorIndexZero=*/false>(src, srcRect, *this, targetPosition);
}

void Surface::BlitFromSkipColorIndexZero(const Surface &src, SDL_Rect srcRect, Point targetPosition) const
{
	if (GetRenderer().NeedsFullRedraw() && isScreenTarget) {
		auto &renderer = GetRenderer();
		renderer.SetClipRegion(region.x, region.y, region.w, region.h);
		renderer.BlitSurfaceSkipColorIndexZero(src, srcRect, { targetPosition.x + region.x, targetPosition.y + region.y });
		renderer.ClearClipRegion();
		return;
	}
	SurfaceBlit</*SkipColorIndexZero=*/true>(src, srcRect, *this, targetPosition);
}

} // namespace devilution