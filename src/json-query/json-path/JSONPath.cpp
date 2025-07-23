#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPathExpected.hpp"

#include <vector>
#include <deque>

namespace json_query {

using json_path::Slice;
using json_path::Token;
using json_path::Error;
using json_path::FilterFn;

// ─────────────────────────────────────────────────────────────────────
//  JSONPath implementation
// ─────────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────────
//  evaluateExpected – with error handling
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalResult JSONPath::evaluateExpected(const QJsonDocument& doc) const
{
    const QJsonValue root = doc.isArray() ? QJsonValue(doc.array())
                                          : QJsonValue(doc.object());
    return evaluateExpected(root);
}

JSONPath::EvalResult JSONPath::evaluateExpected(const QJsonValue& value) const
{
    try {
        json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_contextFilters, value, m_option, m_func};
        QJsonValue result = json_path::detail::evaluate(ctx, value);
        return result;
    } catch (...) {
        // For now, return a generic error if evaluation fails
        // In the future, we could extend the evaluation functions to return std::expected
        return std::unexpected(json_path::EvalError::TypeMismatchObject);
    }
}

// ─────────────────────────────────────────────────────────────────────
//  evaluateAllExpected – array results with error handling
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalArrayResult JSONPath::evaluateAllExpected(const QJsonDocument& doc) const
{
    const QJsonValue root = doc.isArray() ? QJsonValue(doc.array())
                                          : QJsonValue(doc.object());
    return evaluateAllExpected(root);
}

JSONPath::EvalArrayResult JSONPath::evaluateAllExpected(const QJsonValue& value) const
{
    try {
        json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_contextFilters, value, m_option, m_func};
        QJsonArray result = json_path::detail::evaluateAll(ctx, value);
        return result;
    } catch (...) {
        // For now, return a generic error if evaluation fails
        // In the future, we could extend the evaluation functions to return std::expected
        return std::unexpected(json_path::EvalError::TypeMismatchObject);
    }
}

} // namespace json_query
