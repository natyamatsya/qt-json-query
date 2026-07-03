// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "json-query/json-pointer/JSONPointerParsing.hpp"
#include <vector>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_pointer::detail
{

/**
 * @brief Evaluation context for JSON Pointer operations
 *
 * Contains the parsed tokens and provides access to them during evaluation.
 */
struct PointerEvalCtx
{
    explicit PointerEvalCtx(const std::vector<Token>& t) noexcept : tokens{t} {}

    // Core data
    const std::vector<Token>& tokens;
};

} // namespace json_query::json_pointer::detail
