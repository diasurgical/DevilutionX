#pragma once

#include <cstddef>
#include <span>
#include <string_view>
#include <type_traits>

#include <SheenBidi/SheenBidi.h>

namespace devilution::sb {

template <typename T>
using SheenBidiRetainFn = T (*)(T);
template <typename T>
using SheenBidiReleaseFn = void (*)(T);

template <typename T, SheenBidiRetainFn<T> Retain, SheenBidiReleaseFn<T> Release>
class SheenBidiPtr {
public:
	SheenBidiPtr()
	    : ptr_(nullptr)
	{
	}
	explicit SheenBidiPtr(T ptr)
	    : ptr_(ptr)
	{
	}

	SheenBidiPtr(const SheenBidiPtr &other)
	    : ptr_(other.ptr_)
	{
		if (ptr_ != nullptr) Retain(ptr_);
	}

	SheenBidiPtr(SheenBidiPtr &&other) noexcept
	    : ptr_(other.ptr_)
	{
		other.ptr_ = nullptr;
	}

	SheenBidiPtr &operator=(const SheenBidiPtr &other)
	{
		if (this != &other) {
			if (ptr_ != nullptr) Release(ptr_);
			ptr_ = other.ptr_;
			if (ptr_ != nullptr) Retain(ptr_);
		}
		return *this;
	}

	SheenBidiPtr &operator=(SheenBidiPtr &&other) noexcept
	{
		if (this != &other) {
			if (ptr_ != nullptr) Release(ptr_);
			ptr_ = other.ptr_;
			other.ptr_ = nullptr;
		}
		return *this;
	}

	~SheenBidiPtr()
	{
		if (ptr_ != nullptr) Release(ptr_);
	}

	[[nodiscard]] T get() const { return ptr_; }
	T operator->() const { return ptr_; }
	[[nodiscard]] std::remove_pointer_t<T> &operator*() const { return *ptr_; }

	[[nodiscard]] bool operator==(std::nullptr_t) const { return ptr_ == nullptr; }
	[[nodiscard]] bool operator!=(std::nullptr_t) const { return ptr_ != nullptr; }

	[[nodiscard]] friend bool operator==(std::nullptr_t, const SheenBidiPtr &p) { return p.ptr_ == nullptr; }
	[[nodiscard]] friend bool operator!=(std::nullptr_t, const SheenBidiPtr &p) { return p.ptr_ != nullptr; }

	[[nodiscard]] explicit operator bool() const { return ptr_ != nullptr; }

private:
	T ptr_;
};

/**
 * @brief Creates a codepoint sequence from a UTF-8 encoded string view.
 *
 * @param text The UTF-8 encoded string.
 * @return A SBCodepointSequence structure.
 */
[[nodiscard]] inline SBCodepointSequence CreateCodepointSequence(std::string_view text)
{
	return SBCodepointSequence { SBStringEncodingUTF8, text.data(), text.size() };
}

class Paragraph;

/**
 * @brief An algorithm object that applies the bidirectional algorithm to a code point sequence.
 */
class Algorithm : public SheenBidiPtr<SBAlgorithmRef, SBAlgorithmRetain, SBAlgorithmRelease> {
public:
	using SheenBidiPtr::SheenBidiPtr;

	/**
	 * @brief Creates an algorithm object for the specified code point sequence.
	 *
	 * The source string inside the code point sequence should not be freed until the algorithm object is in use.
	 *
	 * @param sequence The code point sequence to apply bidirectional algorithm on.
	 * @return A reference to an algorithm object.
	 */
	[[nodiscard]] static Algorithm create(const SBCodepointSequence &sequence)
	{
		return Algorithm(SBAlgorithmCreate(&sequence));
	}

	/**
	 * @brief Creates an algorithm object for the specified text.
	 *
	 * The source string should not be freed until the algorithm object is in use.
	 *
	 * @param text The text to apply bidirectional algorithm on.
	 * @return A reference to an algorithm object.
	 */
	[[nodiscard]] static Algorithm create(std::string_view text)
	{
		return create(CreateCodepointSequence(text));
	}

	/**
	 * @brief Creates a paragraph object processed with Unicode Bidirectional Algorithm.
	 *
	 * This function processes only first paragraph starting at start with length less than or
	 * equal to length, in accordance with Rule P1 of Unicode Bidirectional Algorithm.
	 *
	 * The paragraph level is determined by applying Rules P2-P3 and embedding levels are resolved by
	 * applying Rules X1-I2.
	 *
	 * @param start The index to the first code unit of the paragraph in source string.
	 * @param length The number of code units covering the suggested length of the paragraph.
	 * @param level The desired base level of the paragraph. Rules P2-P3 would be ignored if it is neither
	 *      SBLevelDefaultLTR nor SBLevelDefaultRTL.
	 * @return A reference to a paragraph object.
	 */
	[[nodiscard]] Paragraph createParagraph(SBUInteger start, SBUInteger length, SBLevel level) const;
};

class Line;

/**
 * @brief A paragraph object processed with Unicode Bidirectional Algorithm.
 */
class Paragraph : public SheenBidiPtr<SBParagraphRef, SBParagraphRetain, SBParagraphRelease> {
public:
	using SheenBidiPtr::SheenBidiPtr;

	/**
	 * @brief Creates a paragraph object processed with Unicode Bidirectional Algorithm.
	 *
	 * @param algorithm The algorithm object to use for creating the desired paragraph.
	 * @param start The index to the first code unit of the paragraph in source string.
	 * @param length The number of code units covering the suggested length of the paragraph.
	 * @param level The desired base level of the paragraph.
	 * @return A reference to a paragraph object.
	 */
	[[nodiscard]] static Paragraph create(SBAlgorithmRef algorithm, SBUInteger start, SBUInteger length, SBLevel level)
	{
		return Paragraph(SBAlgorithmCreateParagraph(algorithm, start, length, level));
	}

	/**
	 * @brief Creates a line object of specified range by applying rules L1-L2 of Unicode Bidirectional Algorithm.
	 *
	 * @param start The index to the first code unit of the line in source string. It should occur within the range of paragraph.
	 * @param length The number of code units covering the length of the line.
	 * @return A reference to a line object.
	 */
	[[nodiscard]] Line createLine(SBUInteger start, SBUInteger length) const;
};

inline Paragraph Algorithm::createParagraph(SBUInteger start, SBUInteger length, SBLevel level) const
{
	return Paragraph::create(get(), start, length, level);
}

/**
 * @brief A line object that contains visual runs.
 */
class Line : public SheenBidiPtr<SBLineRef, SBLineRetain, SBLineRelease> {
public:
	using SheenBidiPtr::SheenBidiPtr;

	/**
	 * @brief Creates a line object of specified range by applying rules L1-L2 of Unicode Bidirectional Algorithm.
	 *
	 * @param paragraph The paragraph that creates the line.
	 * @param start The index to the first code unit of the line in source string.
	 * @param length The number of code units covering the length of the line.
	 * @return A reference to a line object.
	 */
	[[nodiscard]] static Line create(SBParagraphRef paragraph, SBUInteger start, SBUInteger length)
	{
		return Line(SBParagraphCreateLine(paragraph, start, length));
	}

	/**
	 * @brief Returns the runs in the line.
	 *
	 * @return A span of SBRun structures.
	 */
	[[nodiscard]] std::span<const SBRun> runs() const
	{
		return std::span<const SBRun>(runsPtr(), runCount());
	}

	/**
	 * @brief Returns the number of runs in the line.
	 *
	 * @return The number of runs in the line.
	 */
	[[nodiscard]] size_t runCount() const { return static_cast<size_t>(SBLineGetRunCount(get())); }

	/**
	 * @brief Returns a direct pointer to the run array, stored in the line.
	 *
	 * @return A valid pointer to an array of SBRun structures.
	 */
	[[nodiscard]] const SBRun *runsPtr() const { return SBLineGetRunsPtr(get()); }
};

inline Line Paragraph::createLine(SBUInteger start, SBUInteger length) const
{
	return Line::create(get(), start, length);
}

/**
 * @brief Checks if a run is right-to-left.
 *
 * @param run The run to check.
 * @return True if the run is right-to-left, false otherwise.
 */
[[nodiscard]] inline bool IsRTL(const SBRun &run) { return (run.level & 1) == 1; }

/**
 * @brief Decodes the next Unicode code point from a UTF-8 encoded string.
 *
 * @param input The string containing UTF-8 encoded code units.
 * @param index The index at which decoding starts. On output, it is updated to the start of the next code point.
 * @return The decoded code point, or SBCodepointInvalid if index is out of bounds.
 */
[[nodiscard]] inline SBCodepoint CodepointDecodeNext(std::string_view input, SBUInteger &index)
{
	return SBCodepointDecodeNextFromUTF8(
	    reinterpret_cast<const SBUInt8 *>(input.data()), static_cast<SBUInteger>(input.size()), &index);
}

/**
 * @brief Decodes the previous Unicode code point from a UTF-8 encoded string.
 *
 * @param input The string containing UTF-8 encoded code units.
 * @param index The index before which decoding occurs. On output, it is updated to the start of the decoded code point.
 * @return The decoded code point, or SBCodepointInvalid if index is zero or out of bounds.
 */
[[nodiscard]] inline SBCodepoint CodepointDecodePrevious(std::string_view input, SBUInteger &index)
{
	return SBCodepointDecodePreviousFromUTF8(
	    reinterpret_cast<const SBUInt8 *>(input.data()), static_cast<SBUInteger>(input.size()), &index);
}

} // namespace devilution::sb
