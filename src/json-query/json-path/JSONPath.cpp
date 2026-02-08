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

    return json_path::detail::evaluate(ctx, value)
        .or_else([](const json_path::detail::DetailedEvalError& e) -> EvalResult
                 { return std::unexpected(json_query::Error{e.error, e.tokenIndex}); });
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

    return json_path::detail::evaluateAll(ctx, value)
        .or_else([](const json_path::detail::DetailedEvalError& e) -> EvalArrayResult
                 { return std::unexpected(json_query::Error{e.error, e.tokenIndex}); });
}

} // namespace json_query::json_path
