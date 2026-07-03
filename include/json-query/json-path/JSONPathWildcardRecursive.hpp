// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "JSONPathError.hpp"

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <expected>
#include <QStringView>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path::detail
{

// ---------------------------------------------------------------------------
//  Wildcard evaluation
// ---------------------------------------------------------------------------

// Wildcard evaluation for objects and arrays
std::expected<QJsonArray, EvalError> wildcardObject(const QJsonObject& obj);
std::expected<QJsonArray, EvalError> wildcardArray(const QJsonArray& arr);

// ---------------------------------------------------------------------------
//  Recursive descent evaluation
// ---------------------------------------------------------------------------

// Recursive descent evaluation (.. operator)
std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, QStringView pathHint = QStringView());

// Backward compatibility overload
std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, int unused);

} // namespace json_query::json_path::detail
