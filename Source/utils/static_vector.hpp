#pragma once

#include <cstddef>
#include <initializer_list>
#include <memory>
#include <utility>

#include "appfat.h"
#include "utils/attributes.h"

namespace devilution {

/**
 * @brief A stack-allocated vector with a fixed capacity.
 *
 * @tparam T element type.
 * @tparam N capacity.
 */
template <class T, size_t N>
class StaticVector {
public:
	using value_type = T;
	using reference = T &;
	using const_reference = const T &;
	using pointer = T *;
	using const_pointer = const T *;
	using size_type = size_t;
	using iterator = T *;
	using const_iterator = const T *;
	using difference_type = std::ptrdiff_t;

	StaticVector() = default;

	template <typename U>
	StaticVector(std::initializer_list<U> elements)
	{
		for (auto &&element : elements) {
			emplace_back(element);
		}
	}

	[[nodiscard]] const T *begin() const { return &(*this)[0]; }
	[[nodiscard]] T *begin() { return &(*this)[0]; }

	[[nodiscard]] const T *end() const { return begin() + size_; }
	[[nodiscard]] T *end() { return begin() + size_; }

	[[nodiscard]] size_t size() const { return size_; }

	[[nodiscard]] bool empty() const DVL_PURE { return size_ == 0; }

	[[nodiscard]] const T &front() const { return (*this)[0]; }
	[[nodiscard]] T &front() { return (*this)[0]; }

	[[nodiscard]] const T &back() const { return (*this)[size_ - 1]; }
	[[nodiscard]] T &back() { return (*this)[size_ - 1]; }

	template <typename... Args>
	void push_back(Args &&...args) // NOLINT(readability-identifier-naming)
	{
		emplace_back(std::forward<Args>(args)...);
	}

	template <typename... Args>
	T &emplace_back(Args &&...args) // NOLINT(readability-identifier-naming)
	{
		assert(size_ < N);
		return *::new (&data_[size_++]) T(std::forward<Args>(args)...);
	}

	const T &operator[](std::size_t pos) const { return *data_[pos].ptr(); }
	T &operator[](std::size_t pos) { return *data_[pos].ptr(); }

	void erase(T *begin, T *end)
	{
		size_t count = end - begin;
		if (empty() || begin >= end || (size() - count) > size()) {
			return;
		}

		std::move(end, this->end(), begin);
		for (const T *it = this->end() - count; it < this->end(); ++it) {
			std::destroy_at(it);
		}

		size_ -= count;
	}

	void pop_back() // NOLINT(readability-identifier-naming)
	{
		std::destroy_at(&back());
		--size_;
	}

	void clear()
	{
		if (empty()) {
			return;
		}
		erase(begin(), end());
	}

	~StaticVector()
	{
		for (std::size_t pos = 0; pos < size_; ++pos) {
			std::destroy_at(data_[pos].ptr());
		}
	}

private:
	struct AlignedStorage {
		alignas(alignof(T)) std::byte data[sizeof(T)];

		const T *ptr() const
		{
			return std::launder(reinterpret_cast<const T *>(data));
		}

		T *ptr()
		{
			return std::launder(reinterpret_cast<T *>(data));
		}
	};
	AlignedStorage data_[N];
	std::size_t size_ = 0;
};

} // namespace devilution
