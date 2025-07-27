#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPathExpected.hpp"

#include <vector>
#include <deque>

namespace json_query
{

using json_path::Error;
using json_path::FilterFn;
using json_path::Slice;
using json_path::Token;

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

    // C++23 Monadic Chain - Elegant error propagation without manual checks!
    return json_path::detail::evaluate(ctx, value)
        .or_else([](json_path::EvalError error) -> EvalResult { return std::unexpected(error); });
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

    // C++23 Monadic Chain - Elegant error propagation without manual checks!
    return json_path::detail::evaluateAll(ctx, value)
        .or_else([](json_path::EvalError error) -> EvalArrayResult { return std::unexpected(error); });
}

} // namespace json_query
