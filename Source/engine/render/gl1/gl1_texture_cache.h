/**
 * @file gl1_texture_cache.h
 *
 * Texture cache for the OpenGL 1.x rendering backend.
 *
 * Sprites and tiles are palette-expanded to RGBA on first use,
 * uploaded as GL textures, and cached by a composite key.
 */
#pragma once

#ifdef DEVILUTIONX_GL1_RENDERER

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "engine/clx_sprite.hpp"
#include "engine/render/dun_render.hpp"

namespace devilution {

/**
 * @brief A cached OpenGL texture with metadata.
 */
struct Gl1CachedTexture {
	uint32_t textureId = 0;
	uint16_t width = 0;
	uint16_t height = 0;
	/** If true, this texture uses palette indices in animated ranges and needs re-upload on palette cycle. */
	bool needsPaletteUpdate = false;
	/** The palette generation at which this texture was last uploaded. */
	uint32_t paletteGeneration = 0;
};

/**
 * @brief Composite cache key for sprite textures.
 *
 * Cache key: (pixelData pointer, palette generation, TRN pointer)
 */
struct Gl1SpriteCacheKey {
	const uint8_t *pixelData;
	const uint8_t *trn; // nullptr for untranslated sprites

	bool operator==(const Gl1SpriteCacheKey &other) const
	{
		return pixelData == other.pixelData && trn == other.trn;
	}
};

struct Gl1SpriteCacheKeyHash {
	size_t operator()(const Gl1SpriteCacheKey &key) const noexcept
	{
		// Simple hash combining pointer values
		auto h1 = std::hash<const void *> {}(key.pixelData);
		auto h2 = std::hash<const void *> {}(key.trn);
		return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
	}
};

/**
 * @brief Composite cache key for tile textures.
 *
 * Uses the raw frame data pointer as the key since RenderTileFrame
 * doesn't have access to LevelCelBlock.
 */
struct Gl1TileCacheKey {
	const uint8_t *frameData;
	TileType tileType;
	MaskType maskType;
	const uint8_t *tbl; // Light table pointer (nullptr = fully lit / vertex lighting)

	bool operator==(const Gl1TileCacheKey &other) const
	{
		return frameData == other.frameData && tileType == other.tileType && maskType == other.maskType && tbl == other.tbl;
	}
};

struct Gl1TileCacheKeyHash {
	size_t operator()(const Gl1TileCacheKey &key) const noexcept
	{
		auto h1 = std::hash<const void *> {}(key.frameData);
		auto h2 = std::hash<uint8_t> {}(static_cast<uint8_t>(key.tileType));
		auto h3 = std::hash<uint8_t> {}(static_cast<uint8_t>(key.maskType));
		auto h4 = std::hash<const void *> {}(key.tbl);
		return h1 ^ (h2 * 0x9e3779b9 + (h1 << 6) + (h1 >> 2)) ^ (h3 * 0x517cc1b727220a95ULL) ^ (h4 * 0x6c62272e07bb0142ULL);
	}
};

/**
 * @brief The global texture cache for the GL1 renderer.
 */
class Gl1TextureCache {
public:
	/**
	 * @brief Get or create a cached texture for a CLX sprite.
	 * @param clx The sprite data.
	 * @param trn Optional TRN color translation (nullptr for none).
	 * @return Reference to the cached texture entry.
	 */
	Gl1CachedTexture &GetSpriteTexture(ClxSprite clx, const uint8_t *trn);

	/**
	 * @brief Get or create a cached texture for a dungeon tile.
	 * @param frameData Raw frame data pointer (from GetDunFrame).
	 * @param tileType The tile type (square, triangle, etc.).
	 * @param maskType The transparency mask type.
	 * @param height Frame height in pixels.
	 * @return Reference to the cached texture entry.
	 */
	Gl1CachedTexture &GetTileTexture(const uint8_t *frameData,
	    TileType tileType, MaskType maskType, int_fast16_t height,
	    const uint8_t *tbl = nullptr);

	/**
	 * @brief Bump the palette generation counter, marking animated entries for re-upload.
	 */
	void NotifyPaletteChanged();

	/**
	 * @brief Mark ALL cached entries as needing re-upload (full palette change).
	 * Used when the entire palette changes (level load, fade, brightness adjustment).
	 * Safe to call from any thread — the actual GL texture deletion is deferred
	 * to the next GetSpriteTexture/GetTileTexture call on the main thread.
	 */
	void NotifyFullPaletteChanged();

	/**
	 * @brief If a full palette change was flagged, perform the actual cache clear.
	 * Must be called on the main (GL) thread before accessing the cache.
	 */
	void FlushIfNeeded();

	/**
	 * @brief Delete all GL textures and clear the cache.
	 */
	void Clear();

	/**
	 * @brief Get the current palette generation counter.
	 */
	uint32_t GetPaletteGeneration() const { return paletteGeneration_; }

private:
	/**
	 * @brief Decode a CLX sprite to RGBA pixels.
	 * @param clx The sprite to decode.
	 * @param trn Optional TRN (nullptr for none).
	 * @param[out] rgbaOut Output RGBA pixel buffer (resized as needed).
	 * @param[out] hasAnimatedPalette Set to true if any animated palette index is used.
	 */
	void DecodeClxToRgba(ClxSprite clx, const uint8_t *trn, std::vector<uint8_t> &rgbaOut, bool &hasAnimatedPalette);

	/**
	 * @brief Write a single RGBA pixel into the output buffer with bounds checking.
	 */
	void WritePixel(uint8_t *rgbaOut, size_t bufferSize,
	    int x, int y, uint16_t width, uint16_t height,
	    uint8_t paletteIndex, const uint8_t *trn, bool &hasAnimatedPalette);

	/**
	 * @brief Write a single tile pixel (no TRN, caller provides alpha).
	 */
	void WriteTilePixel(uint8_t *rgbaOut, int x, int y,
	    uint16_t width, uint16_t height, uint8_t paletteIndex,
	    uint8_t alpha, const uint8_t *tbl, bool &hasAnimatedPalette);

	/**
	 * @brief Decode a dungeon tile frame to RGBA pixels.
	 * @param frameData Raw frame data pointer.
	 * @param tileType Tile type for decoding.
	 * @param maskType Transparency mask.
	 * @param height Frame height in pixels.
	 * @param[out] rgbaOut Output pixel buffer.
	 * @param[out] outWidth Output texture width.
	 * @param[out] outHeight Output texture height.
	 * @param[out] hasAnimatedPalette Set to true if any animated palette index is used.
	 */
	void DecodeTileToRgba(const uint8_t *frameData,
	    TileType tileType, MaskType maskType, int_fast16_t height,
	    const uint8_t *tbl,
	    std::vector<uint8_t> &rgbaOut, uint16_t &outWidth, uint16_t &outHeight,
	    bool &hasAnimatedPalette);

	/**
	 * @brief Upload RGBA data to a GL texture (creating or updating).
	 */
	void UploadTexture(Gl1CachedTexture &entry, uint16_t width, uint16_t height, const uint8_t *rgbaData);

	std::unordered_map<Gl1SpriteCacheKey, Gl1CachedTexture, Gl1SpriteCacheKeyHash> spriteCache_;
	std::unordered_map<Gl1TileCacheKey, Gl1CachedTexture, Gl1TileCacheKeyHash> tileCache_;
	uint32_t paletteGeneration_ = 0;
	bool fullClearPending_ = false;
};

} // namespace devilution

#endif // DEVILUTIONX_GL1_RENDERER