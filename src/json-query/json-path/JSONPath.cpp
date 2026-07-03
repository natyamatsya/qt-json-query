// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPathEvalHelpers.hpp"
#include "json-query/json-path/internal/JSONPathImpl.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include "json-query/utils/JSONError.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path
{

using detail::normalizeIndex;

// ─────────────────────────────────────────────────────────────────────
//  Pimpl boilerplate
// ─────────────────────────────────────────────────────────────────────

JSONPath::JSONPath(std::unique_ptr<const detail::JSONPathImpl> impl) noexcept : m_impl(std::move(impl)) {}

JSONPath::JSONPath(JSONPath&&) noexcept            = default;
JSONPath& JSONPath::operator=(JSONPath&&) noexcept = default;

JSONPath::JSONPath(const JSONPath& other) : m_impl(std::make_unique<detail::JSONPathImpl>(*other.m_impl)) {}

JSONPath& JSONPath::operator=(const JSONPath& other)
{
    if (this != &other)
        m_impl = std::make_unique<detail::JSONPathImpl>(*other.m_impl);
    return *this;
}

JSONPath::~JSONPath() = default;

QString JSONPath::to_string() const { return m_impl->originalPath; }

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
//  evaluateSingle – single value
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalResult JSONPath::evaluateSingle(const QJsonDocument& doc) const
{
    const auto root{doc.isArray() ? QJsonValue{doc.array()} : QJsonValue{doc.object()}};
    return evaluateSingle(root);
}

JSONPath::EvalResult JSONPath::evaluateSingle(const QJsonValue& value) const
{
    const auto& d{*m_impl};
    if (d.definite && d.func == FunctionType::None)
    {
        if (d.tokens.size() <= 1)
            return value;
        auto result{evaluateDefiniteValue(d.tokens, value)};
        if (result.isUndefined())
            return QJsonValue{QJsonArray{}};
        return result;
    }

    json_path::detail::PathEvalCtx ctx{d.tokens, value, d.func};

    return json_path::detail::evaluateSingle(ctx, value)
        .or_else([](const json_path::detail::DetailedEvalError& e) -> EvalResult
                 { return std::unexpected(json_query::Error{e.error, e.tokenIndex}); });
}

// ─────────────────────────────────────────────────────────────────────
//  evaluate – array results
// ─────────────────────────────────────────────────────────────────────

JSONPath::EvalArrayResult JSONPath::evaluate(const QJsonDocument& doc) const
{
    const auto root{doc.isArray() ? QJsonValue{doc.array()} : QJsonValue{doc.object()}};
    return evaluate(root);
}

// Thread-local reusable single-element QJsonArray for definite path evaluation.
// See docs/adr/ADR-001-thread-local-jsonarray-cache.md for rationale.
static QJsonArray& reusableSingleElementArray()
{
    thread_local auto arr{QJsonArray{QJsonValue{}}};
    return arr;
}

JSONPath::EvalArrayResult JSONPath::evaluate(const QJsonValue& value) const
{
    const auto& d{*m_impl};
    if (d.definite && d.func == FunctionType::None)
    {
        if (d.tokens.size() <= 1)
        {
            auto& arr{reusableSingleElementArray()};
            arr[0] = value;
            return arr;
        }
        auto result{evaluateDefiniteValue(d.tokens, value)};
        if (result.isUndefined())
            return QJsonArray{};
        auto& arr{reusableSingleElementArray()};
        arr[0] = result;
        return arr;
    }

    json_path::detail::PathEvalCtx ctx{d.tokens, value, d.func};

    return json_path::detail::evaluate(ctx, value)
        .or_else([](const json_path::detail::DetailedEvalError& e) -> EvalArrayResult
                 { return std::unexpected(json_query::Error{e.error, e.tokenIndex}); });
}

} // namespace json_query::json_path
