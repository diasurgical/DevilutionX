#pragma once

#include <cstdint>
#include <cstring>
#include <execution>
#include <version>

#include "engine/palette.h"
#include "engine/render/light_render.hpp"
#include "utils/attributes.h"

namespace devilution {

#if __cpp_lib_execution >= 201902L
#define DEVILUTIONX_BLIT_EXECUTION_POLICY std::execution::unseq,
#else
#define DEVILUTIONX_BLIT_EXECUTION_POLICY
#endif

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitFillDirect(uint8_t *dst, unsigned length, uint8_t color)
{
	DVL_ASSUME(length != 0);
	std::memset(dst, color, length);
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsDirect(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length)
{
	DVL_ASSUME(length != 0);
	std::memcpy(dst, src, length);
}

struct BlitDirect {
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsDirect(dst, src, length);
	}
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, uint8_t *DVL_RESTRICT dst) const
	{
		BlitFillDirect(dst, length, color);
	}
};

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitFillWithMap(uint8_t *dst, unsigned length, uint8_t color, const uint8_t *DVL_RESTRICT colorMap)
{
	DVL_ASSUME(length != 0);
	std::memset(dst, colorMap[color], length);
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsWithMap(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length, const uint8_t *DVL_RESTRICT colorMap)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, [colorMap](uint8_t srcColor) { return colorMap[srcColor]; });
}

struct BlitWithMap {
	const uint8_t *DVL_RESTRICT colorMap;

	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsWithMap(dst, src, length, colorMap);
	}
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, uint8_t *DVL_RESTRICT dst) const
	{
		BlitFillWithMap(dst, length, color, colorMap);
	}
};

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitFillWithLightmap(uint8_t *dst, unsigned length, uint8_t color, const Lightmap &lightmap)
{
	DVL_ASSUME(length != 0);
	const uint8_t *light = lightmap.getLightingAt(dst);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY light, light + length, dst, [color, &lightmap](uint8_t lightLevel) {
		return lightmap.adjustColor(color, lightLevel);
	});
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsWithLightmap(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length, const Lightmap &lightmap)
{
	DVL_ASSUME(length != 0);
	const uint8_t *light = lightmap.getLightingAt(dst);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, light, dst, [&lightmap](uint8_t srcColor, uint8_t lightLevel) {
		return lightmap.adjustColor(srcColor, lightLevel);
	});
}

struct BlitWithLightmap {
	const Lightmap &lightmap;

	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsWithLightmap(dst, src, length, lightmap);
	}
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, uint8_t *DVL_RESTRICT dst) const
	{
		BlitFillWithLightmap(dst, length, color, lightmap);
	}
};

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitFillBlended(uint8_t *dst, unsigned length, uint8_t color)
{
	DVL_ASSUME(length != 0);
	std::for_each(DEVILUTIONX_BLIT_EXECUTION_POLICY dst, dst + length, [tbl = paletteTransparencyLookup[color]](uint8_t &dstColor) {
		dstColor = tbl[dstColor];
	});
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsBlended(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, dst, [pal = paletteTransparencyLookup](uint8_t srcColor, uint8_t dstColor) {
		return pal[srcColor][dstColor];
	});
}

struct BlitBlended {
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsBlended(dst, src, length);
	}
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, uint8_t *DVL_RESTRICT dst) const
	{
		BlitFillBlended(dst, length, color);
	}
};

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsBlendedWithMap(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length, const uint8_t *DVL_RESTRICT colorMap)
{
	DVL_ASSUME(length != 0);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, dst, dst, [colorMap, pal = paletteTransparencyLookup](uint8_t srcColor, uint8_t dstColor) {
		return pal[dstColor][colorMap[srcColor]];
	});
}

struct BlitBlendedWithMap {
	const uint8_t *DVL_RESTRICT colorMap;

	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsBlendedWithMap(dst, src, length, colorMap);
	}
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, uint8_t *DVL_RESTRICT dst) const
	{
		BlitFillBlended(dst, length, colorMap[color]);
	}
};

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitFillBlendedWithLightmap(uint8_t *dst, unsigned length, uint8_t color, const Lightmap &lightmap)
{
	DVL_ASSUME(length != 0);
	const uint8_t *light = lightmap.getLightingAt(dst);
	std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY light, light + length, dst, dst, [color, &lightmap, pal = paletteTransparencyLookup](uint8_t lightLevel, uint8_t dstColor) {
		uint8_t srcColor = lightmap.adjustColor(color, lightLevel);
		return pal[srcColor][dstColor];
	});
}

DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void BlitPixelsBlendedWithLightmap(uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src, unsigned length, const Lightmap &lightmap)
{
	DVL_ASSUME(length != 0);
	const uint8_t *light = lightmap.getLightingAt(dst);

	if (length < 1024) {
		uint8_t litSrc[1024];
		std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY src, src + length, light, litSrc, [&lightmap](uint8_t srcColor, uint8_t lightLevel) {
			return lightmap.adjustColor(srcColor, lightLevel);
		});
		std::transform(DEVILUTIONX_BLIT_EXECUTION_POLICY litSrc, litSrc + length, dst, dst, [pal = paletteTransparencyLookup](uint8_t srcColor, uint8_t dstColor) {
			return pal[dstColor][srcColor];
		});
		return;
	}

	for (size_t i = 0; i < length; i++) {
		uint8_t srcColor = src[i];
		uint8_t dstColor = dst[i];
		uint8_t lightLevel = light[i];
		uint8_t litColor = lightmap.adjustColor(srcColor, lightLevel);
		dst[i] = paletteTransparencyLookup[dstColor][litColor];
	}
}

struct BlitBlendedWithLightmap {
	const Lightmap &lightmap;

	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t *DVL_RESTRICT dst, const uint8_t *DVL_RESTRICT src) const
	{
		BlitPixelsBlendedWithLightmap(dst, src, length, lightmap);
	}
	DVL_ALWAYS_INLINE DVL_ATTRIBUTE_HOT void operator()(unsigned length, uint8_t color, uint8_t *DVL_RESTRICT dst) const
	{
		BlitFillBlendedWithLightmap(dst, length, color, lightmap);
	}
};

} // namespace devilution
