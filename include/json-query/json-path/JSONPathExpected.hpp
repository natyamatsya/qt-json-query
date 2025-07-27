#pragma once

#include <QJsonValue>
#include <vector>
#include <expected>
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/JSONPathCompile.hpp" // Token

namespace json_query::json_path::detail {

// Evaluate a *definite* JSONPath (no wildcard/recursive/filter) sequentially
// and return either the resulting value or an EvalError.
std::expected<QJsonValue, EvalError>
evaluateDefinite(const std::vector<Token>& tokens, const QJsonValue& root) noexcept;

} // namespace json_query::json_path::detail
