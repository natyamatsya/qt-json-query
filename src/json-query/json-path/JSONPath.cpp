#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPathExpected.hpp"
#include "json-query/json-path/internal/PassPipeline.hpp"

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
//  evaluate – with error handling
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalResult JSONPath::evaluate(const QJsonDocument& doc) const
{
    const QJsonValue root = doc.isArray() ? QJsonValue(doc.array())
                                          : QJsonValue(doc.object());
    return evaluate(root);
}

JSONPath::EvalResult JSONPath::evaluate(const QJsonValue& value) const
{
    try {
        json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_contextFilters, value, m_func};
        auto result = json_path::detail::evaluate(ctx, value);
        if (result) {
            return *result;
        } else {
            return std::unexpected(result.error());
        }
    } catch (...) {
        return std::unexpected(json_path::EvalError::TypeMismatchObject);
    }
}

// ─────────────────────────────────────────────────────────────────────
//  evaluateAll – array results with error handling
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalArrayResult JSONPath::evaluateAll(const QJsonDocument& doc) const
{
    const QJsonValue root = doc.isArray() ? QJsonValue(doc.array())
                                          : QJsonValue(doc.object());
    return evaluateAll(root);
}

JSONPath::EvalArrayResult JSONPath::evaluateAll(const QJsonValue& value) const
{
    try {
        json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_contextFilters, value, m_func};
        auto result = json_path::detail::evaluateAll(ctx, value);
        if (result) {
            return *result;
        } else {
            return std::unexpected(result.error());
        }
    } catch (...) {
        return std::unexpected(json_path::EvalError::TypeMismatchObject);
    }
}

} // namespace json_query
