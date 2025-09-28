#pragma once
/**
 * @file clx_sprite.hpp
 *
 * @brief CLX format sprites.
 *
 * CLX is a format used for DevilutionX graphics at runtime.
 * CLX encodes pixel in the same way CL2 but encodes metadata differently.
 *
 * Unlike CL2:
 *
 * 1. CLX frame header stores frame width and height.
 * 2. CLX frame header does not store 32-pixel block offsets.
 *
 * CLX frame header is 6 bytes:
 *
 *  Bytes |   Type   | Value
 * :-----:|:--------:|-------------
 *  0..2  | uint16_t | header size
 *  2..4  | uint16_t | width
 *  4..6  | uint16_t | height
 *
 * CL2 reference: https://github.com/savagesteel/d1-file-formats/blob/master/PC-Mac/CL2.md#2-file-structure
 */

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <memory>

#include "appfat.h"
#include "utils/endian_read.hpp"
#include "utils/intrusive_optional.hpp"

namespace devilution {

class OptionalClxSprite;

/**
 * @brief A single CLX sprite.
 */
class ClxSprite {
public:
	explicit constexpr ClxSprite(const uint8_t *data, uint32_t dataSize)
	    : data_(data)
	    , pixel_data_size_(dataSize - LoadLE16(data))
	{
		assert(data != nullptr);
	}

	[[nodiscard]] constexpr uint16_t width() const
	{
		return LoadLE16(&data_[2]);
	}

	[[nodiscard]] constexpr uint16_t height() const
	{
		return LoadLE16(&data_[4]);
	}

	/**
	 * @brief The raw pixel data (CL2 frame data).
	 *
	 * Format: https://github.com/savagesteel/d1-file-formats/blob/master/PC-Mac/CL2.md#42-cl2-frame-data
	 */
	[[nodiscard]] constexpr const uint8_t *pixelData() const
	{
		return &data_[LoadLE16(data_)];
	}

	[[nodiscard]] constexpr uint32_t pixelDataSize() const
	{
		return pixel_data_size_;
	}

	constexpr bool operator==(const ClxSprite &other) const
	{
		return data_ == other.data_;
	}

	constexpr bool operator!=(const ClxSprite &other) const
	{
		return !(*this == other);
	}

private:
	// For OptionalClxSprite.
	constexpr ClxSprite() = default;

	const uint8_t *data_ = nullptr;
	uint32_t pixel_data_size_ = 0;

	friend class OptionalClxSprite;
};

class OwnedClxSpriteList;
class OptionalClxSpriteList;
class ClxSpriteListIterator;

/**
 * @brief A list of `ClxSprite`s.
 */
class ClxSpriteList {
public:
	explicit constexpr ClxSpriteList(const uint8_t *data)
	    : data_(data)
	{
		assert(data != nullptr);
	}

	ClxSpriteList(const OwnedClxSpriteList &owned);

	[[nodiscard]] OwnedClxSpriteList clone() const;

	[[nodiscard]] constexpr uint32_t numSprites() const
	{
		return LoadLE32(data_);
	}

	[[nodiscard]] constexpr ClxSprite operator[](size_t spriteIndex) const
	{
		assert(spriteIndex < numSprites());
		const uint32_t begin = spriteOffset(spriteIndex);
		const uint32_t end = spriteOffset(spriteIndex + 1);
		return ClxSprite { &data_[begin], end - begin };
	}

	[[nodiscard]] constexpr uint32_t spriteOffset(size_t spriteIndex) const
	{
		return LoadLE32(&data_[4 + spriteIndex * 4]);
	}

	/** @brief The offset to the next sprite sheet, or file size if this is the last sprite sheet. */
	[[nodiscard]] constexpr uint32_t dataSize() const
	{
		return LoadLE32(&data_[4 + numSprites() * 4]);
	}

	[[nodiscard]] constexpr const uint8_t *data() const
	{
		return data_;
	}

	[[nodiscard]] constexpr ClxSpriteListIterator begin() const;
	[[nodiscard]] constexpr ClxSpriteListIterator end() const;

private:
	// For OptionalClxSpriteList.
	constexpr ClxSpriteList() = default;

	const uint8_t *data_ = nullptr;

	friend class OptionalClxSpriteList;
};

class ClxSpriteListIterator {
public:
	using iterator_category = std::forward_iterator_tag;
	using difference_type = int;
	using value_type = ClxSprite;
	using pointer = void;
	using reference = value_type &;

	constexpr ClxSpriteListIterator(ClxSpriteList list, size_t index)
	    : list_(list)
	    , index_(index)
	{
	}

	constexpr ClxSprite operator*()
	{
		return list_[index_];
	}

	constexpr ClxSpriteListIterator &operator++()
	{
		++index_;
		return *this;
	}

	constexpr ClxSpriteListIterator operator++(int)
	{
		auto copy = *this;
		++(*this);
		return copy;
	}

	constexpr bool operator==(const ClxSpriteListIterator &other) const
	{
		return index_ == other.index_;
	}

	constexpr bool operator!=(const ClxSpriteListIterator &other) const
	{
		return !(*this == other);
	}

private:
	ClxSpriteList list_;
	size_t index_;
};

inline constexpr ClxSpriteListIterator ClxSpriteList::begin() const
{
	return { *this, 0 };
}

inline constexpr ClxSpriteListIterator ClxSpriteList::end() const
{
	return { *this, numSprites() };
}

class OwnedClxSpriteSheet;
class OptionalClxSpriteSheet;
class ClxSpriteSheetIterator;

/**
 * @brief A sprite sheet is a list of `ClxSpriteList`s.
 */
class ClxSpriteSheet {
public:
	explicit constexpr ClxSpriteSheet(const uint8_t *data, uint16_t numLists)
	    : data_(data)
	    , num_lists_(numLists)
	{
		assert(data != nullptr);
		assert(num_lists_ > 0);
	}

	ClxSpriteSheet(const OwnedClxSpriteSheet &owned);

	[[nodiscard]] constexpr uint16_t numLists() const
	{
		return num_lists_;
	}

	[[nodiscard]] constexpr ClxSpriteList operator[](size_t sheetIndex) const
	{
		return ClxSpriteList { &data_[sheetOffset(sheetIndex)] };
	}

	[[nodiscard]] constexpr uint32_t sheetOffset(size_t sheetIndex) const
	{
		assert(sheetIndex < num_lists_);
		return LoadLE32(&data_[4 * sheetIndex]);
	}

	[[nodiscard]] constexpr const uint8_t *data() const
	{
		return data_;
	}

	[[nodiscard]] constexpr ClxSpriteSheetIterator begin() const;
	[[nodiscard]] constexpr ClxSpriteSheetIterator end() const;

	[[nodiscard]] size_t dataSize() const
	{
		return static_cast<size_t>(&data_[sheetOffset(num_lists_ - 1)] + (*this)[num_lists_ - 1].dataSize() - &data_[0]);
	}

private:
	// For OptionalClxSpriteSheet.
	constexpr ClxSpriteSheet() = default;

	const uint8_t *data_ = nullptr;
	uint16_t num_lists_ = 0;

	friend class OptionalClxSpriteSheet;
};

class ClxSpriteSheetIterator {
public:
	using iterator_category = std::forward_iterator_tag;
	using difference_type = int;
	using value_type = ClxSpriteList;
	using pointer = void;
	using reference = value_type &;

	constexpr ClxSpriteSheetIterator(ClxSpriteSheet sheet, size_t index)
	    : sheet_(sheet)
	    , index_(index)
	{
	}

	constexpr ClxSpriteList operator*()
	{
		return sheet_[index_];
	}

	constexpr ClxSpriteSheetIterator &operator++()
	{
		++index_;
		return *this;
	}

	constexpr ClxSpriteSheetIterator operator++(int)
	{
		auto copy = *this;
		++(*this);
		return copy;
	}

	constexpr bool operator==(const ClxSpriteSheetIterator &other) const
	{
		return index_ == other.index_;
	}

	constexpr bool operator!=(const ClxSpriteSheetIterator &other) const
	{
		return !(*this == other);
	}

private:
	ClxSpriteSheet sheet_;
	size_t index_;
};

inline constexpr ClxSpriteSheetIterator ClxSpriteSheet::begin() const
{
	return { *this, 0 };
}

inline constexpr ClxSpriteSheetIterator ClxSpriteSheet::end() const
{
	return { *this, num_lists_ };
}

class OptionalOwnedClxSpriteList;
class OwnedClxSpriteListOrSheet;

/**
 * @brief Implicitly convertible to `ClxSpriteList` and owns its data.
 */
class OwnedClxSpriteList {
public:
	explicit OwnedClxSpriteList(std::unique_ptr<uint8_t[]> &&data)
	    : data_(std::move(data))
	{
		assert(data_ != nullptr);
	}

	OwnedClxSpriteList(OwnedClxSpriteList &&) noexcept = default;
	OwnedClxSpriteList &operator=(OwnedClxSpriteList &&) noexcept = default;

	[[nodiscard]] OwnedClxSpriteList clone() const
	{
		return ClxSpriteList { *this }.clone();
	}

	[[nodiscard]] ClxSprite operator[](size_t spriteIndex) const
	{
		return ClxSpriteList { *this }[spriteIndex];
	}

	[[nodiscard]] uint32_t numSprites() const
	{
		return ClxSpriteList { *this }.numSprites();
	}

	[[nodiscard]] size_t dataSize() const
	{
		return ClxSpriteList { *this }.dataSize();
	}

private:
	// For OptionalOwnedClxSpriteList.
	OwnedClxSpriteList() = default;

	std::unique_ptr<uint8_t[]> data_;

	friend class ClxSpriteList; // for implicit conversion
	friend class OptionalOwnedClxSpriteList;
	friend class OwnedClxSpriteListOrSheet;
};

inline ClxSpriteList::ClxSpriteList(const OwnedClxSpriteList &owned)
    : data_(owned.data_.get())
{
}

inline OwnedClxSpriteList ClxSpriteList::clone() const
{
	const size_t size = dataSize();
	std::unique_ptr<uint8_t[]> data { new uint8_t[size] };
	memcpy(data.get(), data_, size);
	return OwnedClxSpriteList { std::move(data) };
}

/**
 * @brief Implicitly convertible to `ClxSpriteSheet` and owns its data.
 */
class OwnedClxSpriteSheet {
public:
	OwnedClxSpriteSheet(std::unique_ptr<uint8_t[]> &&data, uint16_t numLists)
	    : data_(std::move(data))
	    , num_lists_(numLists)
	{
		assert(data_ != nullptr);
		assert(numLists > 0);
	}

	OwnedClxSpriteSheet(OwnedClxSpriteSheet &&) noexcept = default;
	OwnedClxSpriteSheet &operator=(OwnedClxSpriteSheet &&) noexcept = default;

	[[nodiscard]] ClxSpriteList operator[](size_t sheetIndex) const
	{
		return ClxSpriteSheet { *this }[sheetIndex];
	}

	[[nodiscard]] ClxSpriteSheetIterator begin() const
	{
		return ClxSpriteSheet { *this }.begin();
	}

	[[nodiscard]] ClxSpriteSheetIterator end() const
	{
		return ClxSpriteSheet { *this }.end();
	}

	[[nodiscard]] size_t dataSize() const
	{
		return ClxSpriteSheet { *this }.dataSize();
	}

private:
	// For OptionalOwnedClxSpriteList.
	OwnedClxSpriteSheet() = default;

	std::unique_ptr<uint8_t[]> data_;
	uint16_t num_lists_ = 0;

	friend class ClxSpriteSheet; // for implicit conversion.
	friend class OptionalOwnedClxSpriteSheet;
	friend class OwnedClxSpriteListOrSheet;
};

inline ClxSpriteSheet::ClxSpriteSheet(const OwnedClxSpriteSheet &owned)
    : data_(owned.data_.get())
    , num_lists_(owned.num_lists_)
{
}

class OwnedClxSpriteListOrSheet;
class OptionalClxSpriteListOrSheet;

inline uint16_t GetNumListsFromClxListOrSheetBuffer(const uint8_t *data, size_t size)
{
	const uint32_t maybeNumFrames = LoadLE32(data);

	// If it is a number of frames, then the last frame offset will be equal to the size of the file.
	if (LoadLE32(&data[maybeNumFrames * 4 + 4]) != size)
		return maybeNumFrames / 4;

	// Not a sprite sheet.
	return 0;
}

/**
 * @brief A CLX sprite list or a sprite sheet (list of lists).
 */
class ClxSpriteListOrSheet {
public:
	constexpr ClxSpriteListOrSheet(const uint8_t *data, uint16_t numLists)
	    : data_(data)
	    , num_lists_(numLists)
	{
	}

	ClxSpriteListOrSheet(const OwnedClxSpriteListOrSheet &listOrSheet);

	[[nodiscard]] constexpr ClxSpriteList list() const
	{
		assert(num_lists_ == 0);
		return ClxSpriteList { data_ };
	}

	[[nodiscard]] constexpr ClxSpriteSheet sheet() const
	{
		assert(num_lists_ != 0);
		return ClxSpriteSheet { data_, num_lists_ };
	}

	[[nodiscard]] constexpr bool isSheet() const
	{
		return num_lists_ != 0;
	}

	[[nodiscard]] size_t dataSize() const
	{
		return isSheet() ? sheet().dataSize() : list().dataSize();
	}

private:
	// For OptionalClxSpriteListOrSheet.
	constexpr ClxSpriteListOrSheet() = default;

	const uint8_t *data_ = nullptr;
	uint16_t num_lists_ = 0;

	friend class OptionalClxSpriteListOrSheet;
};

class OptionalOwnedClxSpriteListOrSheet;

/**
 * @brief A CLX sprite list or a sprite sheet (list of lists).
 */
class OwnedClxSpriteListOrSheet {
public:
	static OwnedClxSpriteListOrSheet FromBuffer(std::unique_ptr<uint8_t[]> &&data, size_t size)
	{
		const uint16_t numLists = GetNumListsFromClxListOrSheetBuffer(data.get(), size);
		return OwnedClxSpriteListOrSheet { std::move(data), numLists };
	}

	explicit OwnedClxSpriteListOrSheet(std::unique_ptr<uint8_t[]> &&data, uint16_t numLists)
	    : data_(std::move(data))
	    , num_lists_(numLists)
	{
	}

	explicit OwnedClxSpriteListOrSheet(OwnedClxSpriteSheet &&sheet)
	    : data_(std::move(sheet.data_))
	    , num_lists_(sheet.num_lists_)
	{
	}

	explicit OwnedClxSpriteListOrSheet(OwnedClxSpriteList &&list)
	    : data_(std::move(list.data_))
	    , num_lists_(0)
	{
	}

	[[nodiscard]] ClxSpriteList list() const &
	{
		assert(num_lists_ == 0);
		return ClxSpriteList { data_.get() };
	}

	[[nodiscard]] OwnedClxSpriteList list() &&
	{
		assert(num_lists_ == 0);
		return OwnedClxSpriteList { std::move(data_) };
	}

	[[nodiscard]] ClxSpriteSheet sheet() const &
	{
		assert(num_lists_ != 0);
		return ClxSpriteSheet { data_.get(), num_lists_ };
	}

	[[nodiscard]] OwnedClxSpriteSheet sheet() &&
	{
		assert(num_lists_ != 0);
		return OwnedClxSpriteSheet { std::move(data_), num_lists_ };
	}

	[[nodiscard]] bool isSheet() const
	{
		return num_lists_ != 0;
	}

	[[nodiscard]] uint16_t numLists() const { return num_lists_; }

	[[nodiscard]] size_t dataSize() const
	{
		return ClxSpriteListOrSheet { *this }.dataSize();
	}

private:
	// For OptionalOwnedClxSpriteListOrSheet.
	OwnedClxSpriteListOrSheet() = default;

	std::unique_ptr<uint8_t[]> data_;
	uint16_t num_lists_ = 0;

	friend class ClxSpriteListOrSheet;
	friend class OptionalOwnedClxSpriteListOrSheet;
};

inline ClxSpriteListOrSheet::ClxSpriteListOrSheet(const OwnedClxSpriteListOrSheet &listOrSheet)
    : data_(listOrSheet.data_.get())
    , num_lists_(listOrSheet.num_lists_)
{
}

/**
 * @brief Equivalent to `std::optional<ClxSprite>` but smaller.
 */
class OptionalClxSprite {
	DEFINE_CONSTEXPR_INTRUSIVE_OPTIONAL(OptionalClxSprite, ClxSprite, data_, nullptr)
};

/**
 * @brief Equivalent to `std::optional<ClxSpriteList>` but smaller.
 */
class OptionalClxSpriteList {
	DEFINE_CONSTEXPR_INTRUSIVE_OPTIONAL(OptionalClxSpriteList, ClxSpriteList, data_, nullptr)
};

/**
 * @brief Equivalent to `std::optional<ClxSpriteSheet>` but smaller.
 */
class OptionalClxSpriteSheet {
	DEFINE_CONSTEXPR_INTRUSIVE_OPTIONAL(OptionalClxSpriteSheet, ClxSpriteSheet, data_, nullptr)
};

/**
 * @brief Equivalent to `std::optional<ClxSpriteListOrSheet>` but smaller.
 */
class OptionalClxSpriteListOrSheet {
public:
	DEFINE_INTRUSIVE_OPTIONAL(OptionalClxSpriteListOrSheet, ClxSpriteListOrSheet, data_, nullptr);
};

/**
 * @brief Equivalent to `std::optional<OwnedClxSpriteList>` but smaller.
 */
class OptionalOwnedClxSpriteList {
public:
	DEFINE_INTRUSIVE_OPTIONAL(OptionalOwnedClxSpriteList, OwnedClxSpriteList, data_, nullptr)
};

/**
 * @brief Equivalent to `std::optional<OwnedClxSpriteSheet>` but smaller.
 */
class OptionalOwnedClxSpriteSheet {
public:
	DEFINE_INTRUSIVE_OPTIONAL(OptionalOwnedClxSpriteSheet, OwnedClxSpriteSheet, data_, nullptr)
};

class OptionalOwnedClxSpriteListOrSheet {
public:
	DEFINE_INTRUSIVE_OPTIONAL(OptionalOwnedClxSpriteListOrSheet, OwnedClxSpriteListOrSheet, data_, nullptr);
};

} // namespace devilution
