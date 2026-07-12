#pragma once

#include "utils/attributes.h"

#define RETURN_IF_ERROR(expr)                                         \
	if (auto result = expr; DVL_PREDICT_FALSE(!result.has_value())) { \
		return std::unexpected(std::move(result).error());            \
	}

#define STATUS_MACROS_CONCAT_NAME_INNER(x, y) x##y
#define STATUS_MACROS_CONCAT_NAME(x, y) STATUS_MACROS_CONCAT_NAME_INNER(x, y)

#define ASSIGN_OR_RETURN_IMPL(result, lhs, rhs)                                                        \
	auto result = rhs;                            /* NOLINT(bugprone-macro-parentheses): assignment */ \
	if (DVL_PREDICT_FALSE(!result.has_value())) { /* NOLINT(bugprone-macro-parentheses): assignment */ \
		return std::unexpected(std::move(result).error());                                             \
	}                                                                                                  \
	lhs = std::move(result).value(); /* NOLINT(bugprone-macro-parentheses): assignment */

#define ASSIGN_OR_RETURN(lhs, rhs) \
	ASSIGN_OR_RETURN_IMPL(         \
	    STATUS_MACROS_CONCAT_NAME(_result, __COUNTER__), lhs, rhs)
