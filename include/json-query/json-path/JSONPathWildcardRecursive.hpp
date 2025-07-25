#pragma once

#include "json-query/json-path/JSONPathEvalError.hpp"

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <expected>

namespace json_query::json_path::detail {

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
std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, int unused = 0);

} // namespace json_query::json_path::detail
