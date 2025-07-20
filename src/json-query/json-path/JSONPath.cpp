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

QJsonValue JSONPath::evaluate(const QJsonDocument &document) const
{
    const QJsonValue root = document.isArray() ? QJsonValue(document.array())
                                               : QJsonValue(document.object());
    return evaluate(root);
}

// ─────────────────────────────────────────────────────────────────────
//  evaluate (token pipeline)
// ─────────────────────────────────────────────────────────────────────
QJsonValue JSONPath::evaluate(const QJsonValue& root) const
{
    json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_option, m_func};
    return json_path::detail::evaluate(ctx, root);
}

QJsonArray JSONPath::evaluateAll(const QJsonDocument &document) const
{
    const QJsonValue root = document.isArray() ? QJsonValue(document.array())
                                               : QJsonValue(document.object());
    json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_option, m_func};
    return json_path::detail::evaluateAll(ctx, root);
}

QJsonArray JSONPath::evaluateAll(const QJsonValue &value) const
{
    json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_option, m_func};
    return json_path::detail::evaluateAll(ctx, value);
}

// ─────────────────────────────────────────────────────────────────────
//  evaluateExpected – sequential, definite-path only for now
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalResult JSONPath::evaluateExpected(const QJsonDocument& doc) const
{
    const QJsonValue root = doc.isArray() ? QJsonValue(doc.array())
                                          : QJsonValue(doc.object());
    return evaluateExpected(root);
}

JSONPath::EvalResult JSONPath::evaluateExpected(const QJsonValue& value) const
{
    // Currently handles definite paths only (no wildcard/filter/recursive)
    return json_path::detail::evaluateDefinite(m_tokens, value);
}

} // namespace json_query
