#pragma once

#include <algorithm>
#include <iterator>
#include <type_traits>
#include <utility>

namespace devilution {

// Internal namespace that sets up ADL lookup and the container Iterator type.
namespace container_internal {
using std::begin;
using std::end;

template <typename C>
using Iterator = decltype(begin(std::declval<C &>()));

template <typename C>
using Difference = typename std::iterator_traits<Iterator<C>>::difference_type;

template <typename C>
Iterator<C> c_begin(C &c)
{
	return begin(c);
}

template <typename C>
Iterator<C> c_end(C &c)
{
	return end(c);
}

} // namespace container_internal

template <typename C, typename Predicate>
bool c_any_of(const C &c, Predicate &&predicate)
{
	return std::any_of(container_internal::begin(c),
	    container_internal::end(c),
	    std::forward<Predicate>(predicate));
}

template <typename C, typename Predicate>
bool c_all_of(const C &c, Predicate &&predicate)
{
	return std::all_of(container_internal::begin(c),
	    container_internal::end(c),
	    std::forward<Predicate>(predicate));
}

template <typename C, typename Predicate>
bool c_none_of(const C &c, Predicate &&predicate)
{
	return std::none_of(container_internal::begin(c),
	    container_internal::end(c),
	    std::forward<Predicate>(predicate));
}

template <typename C, typename T>
container_internal::Iterator<C>
c_find(C &c, T &&value)
{
	return std::find(container_internal::begin(c),
	    container_internal::end(c),
	    std::forward<T>(value));
}

template <typename C, typename Predicate>
container_internal::Iterator<C>
c_find_if(C &c, Predicate &&predicate)
{
	return std::find_if(container_internal::begin(c),
	    container_internal::end(c),
	    std::forward<Predicate>(predicate));
}

template <typename C, typename Predicate>
container_internal::Difference<C>
c_count_if(const C &c, Predicate &&predicate)
{
	return std::count_if(container_internal::c_begin(c),
	    container_internal::c_end(c),
	    std::forward<Predicate>(predicate));
}

template <typename C, typename T>
container_internal::Difference<C>
c_count(const C &c, T &&value)
{
	return std::count(container_internal::c_begin(c),
	    container_internal::c_end(c),
	    std::forward<T>(value));
}

template <typename C>
void c_sort(C &c)
{
	std::sort(container_internal::c_begin(c),
	    container_internal::c_end(c));
}

template <typename C, typename LessThan>
void c_sort(C &c, LessThan &&comp)
{
	std::sort(container_internal::c_begin(c),
	    container_internal::c_end(c),
	    std::forward<LessThan>(comp));
}

template <typename C, typename T>
container_internal::Iterator<C> c_lower_bound(C &c, T &&value)
{
	return std::lower_bound(container_internal::c_begin(c),
	    container_internal::c_end(c),
	    std::forward<T>(value));
}

template <typename C>
container_internal::Iterator<C> c_unique(C &c)
{
	return std::unique(container_internal::c_begin(c),
	    container_internal::c_end(c));
}

} // namespace devilution
