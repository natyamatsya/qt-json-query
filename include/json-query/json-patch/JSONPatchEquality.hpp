// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include <QJsonValue>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_patch::detail
{

// RFC 6902 §4.6 value equality, used by the "test" operation.
// QJsonValue::operator== distinguishes the integer and double
// representations Qt's CBOR backend gives 1 and 1.0; the spec compares
// numbers "numerically equal", so numbers are compared as doubles here.
// Recursion depth is bounded by the document depth (Qt's JSON parser
// enforces its own nesting limit). Defined in JSONPatch.cpp.
[[nodiscard]] bool jsonDeepEquals(const QJsonValue& a, const QJsonValue& b) noexcept;

} // namespace json_query::json_patch::detail
