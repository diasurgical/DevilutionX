#pragma once

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string>

#include <SDL.h>

#include "utils/stdcompat/string_view.hpp"
#include "utils/utf8.hpp"

namespace devilution {

/** @brief A range of bytes in text. */
struct TextRange {
	size_t begin = 0;
	size_t end = 0;

	[[nodiscard]] size_t size() const
	{
		return end - begin;
	}

	[[nodiscard]] bool empty() const
	{
		return begin == end;
	}

	void clear()
	{
		begin = end = 0;
	}
};

/**
 * @brief Current state of the cursor and the selection range.
 */
struct TextInputCursorState {
	size_t position = 0;
	TextRange selection;
};

/**
 * @brief Manages state for a single-line text input with a cursor.
 *
 * The text value and the cursor position are stored externally.
 */
class TextInputState {
	/**
	 * @brief Manages an unowned fixed size char array.
	 */
	struct Buffer {
		Buffer(char *begin, size_t maxLength)
		    : buf_(begin)
		    , maxLength_(maxLength)
		{
			string_view str(begin);
			str = TruncateUtf8(str, maxLength);
			len_ = str.size();
			buf_[len_] = '\0';
		}

		[[nodiscard]] size_t size() const
		{
			return len_;
		}

		[[nodiscard]] bool empty() const
		{
			return len_ == 0;
		}

		Buffer &operator=(string_view value)
		{
			value = TruncateUtf8(value, maxLength_);
			CopyUtf8(buf_, value, maxLength_);
			len_ = value.size();
			return *this;
		}

		void insert(size_t pos, string_view value)
		{
			value = truncateForInsertion(value);
			std::memmove(&buf_[pos + value.size()], &buf_[pos], len_ - pos);
			std::memcpy(&buf_[pos], value.data(), value.size());
			len_ += value.size();
			buf_[len_] = '\0';
		}

		void erase(size_t pos, size_t len)
		{
			std::memmove(&buf_[pos], &buf_[pos + len], len_ - (pos + len));
			len_ -= len;
			buf_[len_] = '\0';
		}

		void clear()
		{
			len_ = 0;
			buf_[0] = '\0';
		}

		explicit operator string_view() const
		{
			return { buf_, len_ };
		}

	private:
		/**
		 * @brief Truncates `text` so that it would fit when inserted,
		 * respecting UTF-8 code point boundaries.
		 */
		[[nodiscard]] string_view truncateForInsertion(string_view text) const
		{
			return TruncateUtf8(text, maxLength_ - len_);
		}

		char *buf_; // unowned
		size_t maxLength_;
		size_t len_;
	};

public:
	struct Options {
		char *value;                  // unowned
		TextInputCursorState *cursor; // unowned
		size_t maxLength = 0;
	};
	TextInputState(const Options &options)
	    : value_(options.value, options.maxLength)
	    , cursor_(options.cursor)
	{
		cursor_->position = value_.size();
	}

	[[nodiscard]] string_view value() const
	{
		return string_view(value_);
	}

	[[nodiscard]] string_view selectedText() const
	{
		return value().substr(cursor_->selection.begin, cursor_->selection.size());
	}

	[[nodiscard]] bool empty() const
	{
		return value_.empty();
	}

	[[nodiscard]] size_t cursorPosition() const
	{
		return cursor_->position;
	}

	/**
	 * @brief Overwrites the value with the given text and moves cursor to the end.
	 */
	void assign(string_view text)
	{
		value_ = text;
		cursor_->position = value_.size();
	}

	void clear()
	{
		value_.clear();
		cursor_->position = 0;
	}

	/**
	 * @brief Truncate to precisely `length` bytes.
	 */
	void truncate(size_t length)
	{
		if (length >= value().size())
			return;
		value_ = value().substr(0, length);
		cursor_->position = std::min(cursor_->position, value_.size());
	}

	/**
	 * @brief Erases the currently selected text and sets the cursor to selection start.
	 */
	void eraseSelection()
	{
		value_.erase(cursor_->selection.begin, cursor_->selection.size());
		cursor_->position = cursor_->selection.begin;
		cursor_->selection.clear();
	}

	/**
	 * @brief Inserts the text at the current cursor position.
	 */
	void type(string_view text)
	{
		if (!cursor_->selection.empty())
			eraseSelection();
		const size_t prevSize = value_.size();
		value_.insert(cursor_->position, text);
		cursor_->position += value_.size() - prevSize;
	}

	void backspace(bool word)
	{
		if (cursor_->selection.empty()) {
			if (cursor_->position == 0)
				return;
			cursor_->selection.begin = prevPosition(word);
			cursor_->selection.end = cursor_->position;
		}
		eraseSelection();
	}

	void del(bool word)
	{
		if (cursor_->selection.empty()) {
			if (cursor_->position == value_.size())
				return;
			cursor_->selection.begin = cursor_->position;
			cursor_->selection.end = nextPosition(word);
		}
		eraseSelection();
	}

	void setCursorToStart()
	{
		cursor_->position = 0;
		cursor_->selection.clear();
	}

	void setSelectCursorToStart()
	{
		if (cursor_->selection.empty()) {
			cursor_->selection.end = cursor_->position;
		} else if (cursor_->selection.end == cursor_->position) {
			cursor_->selection.end = cursor_->selection.begin;
		}
		cursor_->selection.begin = cursor_->position = 0;
	}

	void setCursorToEnd()
	{
		cursor_->position = value_.size();
		cursor_->selection.clear();
	}

	void setSelectCursorToEnd()
	{
		if (cursor_->selection.empty()) {
			cursor_->selection.begin = cursor_->position;
		} else if (cursor_->selection.begin == cursor_->position) {
			cursor_->selection.begin = cursor_->selection.end;
		}
		cursor_->selection.end = cursor_->position = value_.size();
	}

	void moveCursorLeft(bool word)
	{
		cursor_->selection.clear();
		if (cursor_->position == 0)
			return;
		const size_t newPosition = prevPosition(word);
		cursor_->position = newPosition;
	}

	void moveSelectCursorLeft(bool word)
	{
		if (cursor_->position == 0)
			return;
		const size_t newPosition = prevPosition(word);
		if (cursor_->selection.empty()) {
			cursor_->selection.begin = newPosition;
			cursor_->selection.end = cursor_->position;
		} else if (cursor_->selection.end == cursor_->position) {
			cursor_->selection.end = newPosition;
		} else {
			cursor_->selection.begin = newPosition;
		}
		cursor_->position = newPosition;
	}

	void moveCursorRight(bool word)
	{
		cursor_->selection.clear();
		if (cursor_->position == value_.size())
			return;
		const size_t newPosition = nextPosition(word);
		cursor_->position = newPosition;
	}

	void moveSelectCursorRight(bool word)
	{
		if (cursor_->position == value_.size())
			return;
		const size_t newPosition = nextPosition(word);
		if (cursor_->selection.empty()) {
			cursor_->selection.begin = cursor_->position;
			cursor_->selection.end = newPosition;
		} else if (cursor_->selection.begin == cursor_->position) {
			cursor_->selection.begin = newPosition;
		} else {
			cursor_->selection.end = newPosition;
		}
		cursor_->position = newPosition;
	}

private:
	[[nodiscard]] static bool isWordSeparator(unsigned char c)
	{
		const bool isAsciiWordChar = (c >= '0' && c <= '9')
		    || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
		return c <= '\x7E' && !isAsciiWordChar;
	}

	[[nodiscard]] size_t prevPosition(bool word) const
	{
		const string_view str = beforeCursor();
		size_t pos = FindLastUtf8Symbols(str);
		if (!word)
			return pos;
		while (pos > 0 && isWordSeparator(str[pos])) {
			pos = FindLastUtf8Symbols({ str.data(), pos });
		}
		while (pos > 0) {
			const size_t prevPos = FindLastUtf8Symbols({ str.data(), pos });
			if (isWordSeparator(str[prevPos]))
				break;
			pos = prevPos;
		}
		return pos;
	}

	[[nodiscard]] size_t nextPosition(bool word) const
	{
		const string_view str = afterCursor();
		size_t pos = Utf8CodePointLen(str.data());
		if (!word)
			return cursor_->position + pos;
		while (pos < str.size() && isWordSeparator(str[pos])) {
			pos += Utf8CodePointLen(str.data() + pos);
		}
		while (pos < str.size()) {
			pos += Utf8CodePointLen(str.data() + pos);
			if (isWordSeparator(str[pos]))
				break;
		}
		return cursor_->position + pos;
	}

	[[nodiscard]] string_view beforeCursor() const
	{
		return value().substr(0, cursor_->position);
	}

	[[nodiscard]] string_view afterCursor() const
	{
		return value().substr(cursor_->position);
	}

	Buffer value_;
	TextInputCursorState *cursor_; // unowned
};

/**
 * @brief Manages state for a number input with a cursor.
 */
class NumberInputState {
public:
	struct Options {
		TextInputState::Options textOptions;
		int min;
		int max;
	};
	NumberInputState(const Options &options)
	    : textInput_(options.textOptions)
	    , min_(options.min)
	    , max_(options.max)
	{
	}

	[[nodiscard]] bool empty() const
	{
		return textInput_.empty();
	}

	[[nodiscard]] int value(int defaultValue = 0) const;

	[[nodiscard]] int max() const
	{
		return max_;
	}

	/**
	 * @brief Inserts the text at the current cursor position.
	 *
	 * Ignores non-numeric characters.
	 */
	void type(string_view str);

	/**
	 * @brief Sets the text of the input.
	 *
	 * Ignores non-numeric characters.
	 */
	void assign(string_view str);

	TextInputState &textInput()
	{
		return textInput_;
	}

private:
	void enforceRange();
	std::string filterStr(string_view str, bool allowMinus);

	TextInputState textInput_;
	int min_;
	int max_;
};

bool HandleTextInputEvent(const SDL_Event &event, TextInputState &state);
bool HandleNumberInputEvent(const SDL_Event &event, NumberInputState &state);

} // namespace devilution
