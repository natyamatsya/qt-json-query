// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "json-query/json-pointer/JSONPointerParsing.hpp"

#include <vector>

namespace json_query::json_pointer::detail
{

/// Compiled JSONPointer state, hidden behind the public pimpl
/// (JSONPointer.hpp). Immutable after construction; shared across copies.
struct JSONPointerImpl
{
    std::vector<Token> tokens;
};

} // namespace json_query::json_pointer::detail
