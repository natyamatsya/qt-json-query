// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/utils/JSONError.hpp"

#include <vector>
#include <deque>

namespace json_query::json_path
{

// ─────────────────────────────────────────────────────────────────────
//  JSONPath implementation
// ─────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────
//  evaluate – with error handling
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalResult JSONPath::evaluate(const QJsonDocument& doc) const
{
    const QJsonValue root = doc.isArray() ? QJsonValue{doc.array()} : QJsonValue{doc.object()};
    return evaluate(root);
}

JSONPath::EvalResult JSONPath::evaluate(const QJsonValue& value) const
{
    json_path::detail::PathEvalCtx ctx{m_tokens, value, m_func};

    // C++23 Monadic Chain - Elegant error propagation with unified Error
    return json_path::detail::evaluate(ctx, value)
        .or_else(
            [](json_path::EvalError error) -> EvalResult
            {
                return std::unexpected(
                    json_query::Error{json_query::ErrorDomain::PathEval, static_cast<std::uint8_t>(error)});
            });
}

// ─────────────────────────────────────────────────────────────────────
//  evaluateAll – array results with error handling
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalArrayResult JSONPath::evaluateAll(const QJsonDocument& doc) const
{
    const QJsonValue root = doc.isArray() ? QJsonValue{doc.array()} : QJsonValue{doc.object()};
    return evaluateAll(root);
}

JSONPath::EvalArrayResult JSONPath::evaluateAll(const QJsonValue& value) const
{
    json_path::detail::PathEvalCtx ctx{m_tokens, value, m_func};

    // C++23 Monadic Chain - Elegant error propagation with unified Error
    return json_path::detail::evaluateAll(ctx, value)
        .or_else(
            [](json_path::EvalError error) -> EvalArrayResult
            {
                return std::unexpected(
                    json_query::Error{json_query::ErrorDomain::PathEval, static_cast<std::uint8_t>(error)});
            });
}

} // namespace json_query::json_path
