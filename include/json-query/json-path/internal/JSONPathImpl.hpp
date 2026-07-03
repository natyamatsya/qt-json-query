// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "json-query/json-path/JSONPathCompile.hpp"

#include <QString>
#include <vector>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path::detail
{

/// Compiled JSONPath state, hidden behind the public pimpl (JSONPath.hpp).
/// Immutable after construction; shared across copies of JSONPath.
struct JSONPathImpl
{
    FunctionType       func{FunctionType::None};
    QString            originalPath;
    std::vector<Token> tokens;
    bool               definite{false};
};

} // namespace json_query::json_path::detail
