// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPathEvalHelpers.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include "json-query/utils/JSONError.hpp"

namespace json_query::json_path
{

using detail::normalizeIndex;

// ─────────────────────────────────────────────────────────────────────
//  Definite path inline evaluation — bypasses entire pipeline
// ─────────────────────────────────────────────────────────────────────

static QJsonValue evaluateDefiniteValue(const std::vector<Token>& tokens, const QJsonValue& root)
{
    auto cur{root};
    for (std::size_t i{1}; i < tokens.size(); ++i)
    {
        const auto& tk{tokens[i]};
        if (tk.kind == Token::Kind::Key)
        {
            if (!cur.isObject())
                return QJsonValue::Undefined;
            const auto obj{cur.toObject()};
            const auto it{obj.constFind(tk.key)};
            if (it == obj.constEnd())
                return QJsonValue::Undefined;
            cur = *it;
        }
        else // Index
        {
            if (!cur.isArray())
                return QJsonValue::Undefined;
            const auto arr{asArray(cur)};
            const auto idx{normalizeIndex(tk.index, arr.size())};
            if (idx < 0 || idx >= arr.size())
                return QJsonValue::Undefined;
            cur = arr[idx];
        }
    }
    return cur;
}

// ─────────────────────────────────────────────────────────────────────
//  evaluate – single value
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalResult JSONPath::evaluate(const QJsonDocument& doc) const
{
    const auto root{doc.isArray() ? QJsonValue{doc.array()} : QJsonValue{doc.object()}};
    return evaluate(root);
}

JSONPath::EvalResult JSONPath::evaluate(const QJsonValue& value) const
{
    if (m_definite && m_func == FunctionType::None)
    {
        if (m_tokens.size() <= 1)
            return value;
        auto result{evaluateDefiniteValue(m_tokens, value)};
        if (result.isUndefined())
            return QJsonValue{QJsonArray{}};
        return result;
    }

    json_path::detail::PathEvalCtx ctx{m_tokens, value, m_func};

    return json_path::detail::evaluate(ctx, value)
        .or_else([](const json_path::detail::DetailedEvalError& e) -> EvalResult
                 { return std::unexpected(json_query::Error{e.error, e.tokenIndex}); });
}

// ─────────────────────────────────────────────────────────────────────
//  evaluateAll – array results
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalArrayResult JSONPath::evaluateAll(const QJsonDocument& doc) const
{
    const auto root{doc.isArray() ? QJsonValue{doc.array()} : QJsonValue{doc.object()}};
    return evaluateAll(root);
}

JSONPath::EvalArrayResult JSONPath::evaluateAll(const QJsonValue& value) const
{
    if (m_definite && m_func == FunctionType::None)
    {
        if (m_tokens.size() <= 1)
            return QJsonArray{value};
        auto result{evaluateDefiniteValue(m_tokens, value)};
        if (result.isUndefined())
            return QJsonArray{};
        return QJsonArray{result};
    }

    json_path::detail::PathEvalCtx ctx{m_tokens, value, m_func};

    return json_path::detail::evaluateAll(ctx, value)
        .or_else([](const json_path::detail::DetailedEvalError& e) -> EvalArrayResult
                 { return std::unexpected(json_query::Error{e.error, e.tokenIndex}); });
}

} // namespace json_query::json_path
