// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "JSONPointerEvaluation.hpp" // DetailedEvalError
#include "JSONPointerParsing.hpp"    // Token

#include <QJsonValue>
#include <expected>
#include <vector>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_pointer::detail
{

// Primitive write operations, matching RFC 6902 §4 semantics. The public
// JSONPointer::set() is Add with createIntermediates.
enum class WriteOp : quint8
{
    Add,
    Remove,
    Replace,
};

// Apply a write operation at the location addressed by `tokens` inside
// `root`, in place. Two-phase walk: a read-only descent validates the whole
// path first, so on any error `root` is guaranteed untouched (strong
// guarantee); only then is the modified spine rebuilt bottom-up (Qt JSON
// containers are copy-on-write value types — untouched siblings stay shared).
//
// Returns the removed value for WriteOp::Remove (Undefined otherwise), or a
// DetailedEvalError carrying the failing token index.
//
// Deliberately defined in its own translation unit (JSONPointerWrite.cpp):
// consumers that never write do not pull this code out of the static archive,
// keeping the read path cost-free.
[[nodiscard]] std::expected<QJsonValue, DetailedEvalError> writePointer(const std::vector<Token>& tokens,
                                                                        QJsonValue&               root,
                                                                        const QJsonValue&         value,
                                                                        WriteOp                   op,
                                                                        bool createIntermediates) noexcept;

} // namespace json_query::json_pointer::detail
