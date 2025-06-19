/**
 * @file attributes.h
 *
 * Macros for attributes on functions, variables, etc.
 */
#pragma once

#ifdef __has_attribute
#define DVL_HAVE_ATTRIBUTE(x) __has_attribute(x)
#else
#define DVL_HAVE_ATTRIBUTE(x) 0
#endif

#ifdef __has_builtin
#define DVL_HAVE_BUILTIN(x) __has_builtin(x)
#else
#define DVL_HAVE_BUILTIN(x) 0
#endif

#if DVL_HAVE_ATTRIBUTE(format) || (defined(__GNUC__) && !defined(__clang__))
#define DVL_PRINTF_ATTRIBUTE(fmtargnum, firstarg) \
	__attribute__((__format__(__printf__, fmtargnum, firstarg)))
#else
#define DVL_PRINTF_ATTRIBUTE(fmtargnum, firstarg)
#endif

#if DVL_HAVE_ATTRIBUTE(pure)
#define DVL_PURE __attribute__((pure))
#else
#define DVL_PURE
#endif

#if DVL_HAVE_ATTRIBUTE(always_inline)
#define DVL_ALWAYS_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define DVL_ALWAYS_INLINE __forceinline
#else
#define DVL_ALWAYS_INLINE inline
#endif

#if DVL_HAVE_ATTRIBUTE(noinline)
#define DVL_NO_INLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define DVL_NO_INLINE __declspec(noinline)
#else
#define DVL_NO_INLINE
#endif

#if DVL_HAVE_ATTRIBUTE(hot)
#define DVL_ATTRIBUTE_HOT __attribute__((hot))
#else
#define DVL_ATTRIBUTE_HOT
#endif

// Any global data used by tests must be marked with `DVL_API_FOR_TEST`.
#if defined(_MSC_VER) && defined(BUILD_TESTING)
#ifdef _DVL_EXPORTING
#define DVL_API_FOR_TEST __declspec(dllexport)
#else
#define DVL_API_FOR_TEST __declspec(dllimport)
#endif
#else
#define DVL_API_FOR_TEST
#endif

#if defined(__clang__)
#define DVL_REINITIALIZES [[clang::reinitializes]]
#elif DVL_HAVE_ATTRIBUTE(reinitializes)
#define DVL_REINITIALIZES __attribute__((reinitializes))
#else
#define DVL_REINITIALIZES
#endif

#if ((defined(__GNUC__) || defined(__clang__)) && !defined(__EXCEPTIONS)) || defined(_MSC_VER) && !_HAS_EXCEPTIONS
// NOLINTNEXTLINE(modernize-macro-to-enum)
#define DVL_EXCEPTIONS 0
#else
// NOLINTNEXTLINE(modernize-macro-to-enum)
#define DVL_EXCEPTIONS 1
#endif

#if defined(_MSC_VER)
#define DVL_RESTRICT __restrict
#else
#define DVL_RESTRICT __restrict__
#endif

#ifdef __has_cpp_attribute
#if __has_cpp_attribute(assume) >= 202207L
#define DVL_ASSUME(...) [[assume(__VA_ARGS__)]]
#endif
#endif
#ifndef DVL_ASSUME
#if defined(__clang__)
#define DVL_ASSUME(...)                \
	do {                               \
		__builtin_assume(__VA_ARGS__); \
	} while (false)
#elif defined(_MSC_VER)
#define DVL_ASSUME(...)        \
	do {                       \
		__assume(__VA_ARGS__); \
	} while (false)
#elif defined(__GNUC__)
#if __GNUC__ >= 13
#define DVL_ASSUME(...) __attribute__((__assume__(__VA_ARGS__)))
#endif
#endif
#endif
#ifndef DVL_ASSUME
#define DVL_ASSUME(...)
#endif

#if defined(__clang__) || defined(__GNUC__)
#define DVL_UNREACHABLE() __builtin_unreachable()
#elif defined(_MSC_VER)
#define DVL_UNREACHABLE() __assume(false)
#else
#define DVL_UNREACHABLE()
#endif

#if DVL_HAVE_BUILTIN(__builtin_expect)
#define DVL_PREDICT_FALSE(x) (__builtin_expect(false || (x), false))
#define DVL_PREDICT_TRUE(x) (__builtin_expect(false || (x), true))
#else
#define DVL_PREDICT_FALSE(x) (x)
#define DVL_PREDICT_TRUE(x) (x)
#endif
