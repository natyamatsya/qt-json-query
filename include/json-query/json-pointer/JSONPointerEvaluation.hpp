// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "JSONPointerParsing.hpp"
#include "internal/PointerEvalCtx.hpp"
#include "json-query/utils/JSONQueryError.hpp"

#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <vector>
#include <expected>

namespace json_query::json_pointer::detail
{

[[nodiscard]] inline bool stepObject(QJsonValue& current, const QString& key) noexcept
{
    const QJsonObject obj{current.toObject()};
    const auto        it{obj.constFind(key)};
    if (it == obj.constEnd())
        return false;
    current = *it;
    return true;
}

[[nodiscard]] inline bool stepArray(QJsonValue& current, qsizetype index) noexcept
{
    const QJsonArray arr{current.toArray()};
    if (index < 0 || index >= arr.size())
        return false;
    current = arr.at(index);
    return true;
}

// Internal implementation that returns domain-specific errors
[[nodiscard]] inline std::expected<QJsonValue, EvalError> evaluatePointerImpl(const std::vector<Token>& tokens,
                                                                              const QJsonValue&         root) noexcept
{
    if (tokens.empty())
        return root; // success with root value

    QJsonValue current{root};
    for (const Token& tk : tokens)
    {
        switch (current.type())
        {
        case QJsonValue::Object:
            if (tk.kind != Token::Kind::Key)
                return std::unexpected(EvalError::TypeMismatchObject);
            if (!stepObject(current, tk.key))
                return std::unexpected(EvalError::KeyNotFound);
            break;
        case QJsonValue::Array:
            if (tk.kind != Token::Kind::Index)
                return std::unexpected(EvalError::TypeMismatchArray);
            if (!stepArray(current, tk.index))
                return std::unexpected(EvalError::IndexOutOfRange);
            break;
        default:
            return std::unexpected(EvalError::TypeMismatchObject);
        }
    }
    return current; // success
}

// Public API that converts domain errors to QueryError
[[nodiscard]] inline std::expected<QJsonValue, json_query::QueryError>
evaluatePointer(const std::vector<Token>& tokens, const QJsonValue& root) noexcept
{
    auto result = evaluatePointerImpl(tokens, root);
    if (!result)
        return std::unexpected(json_query::QueryError{result.error()});
    return *result;
}

// Convenience overload taking a PointerEvalCtx to mirror JSONPath's API
[[nodiscard]] inline std::expected<QJsonValue, json_query::QueryError> evaluatePointer(const PointerEvalCtx& ctx,
                                                                                       const QJsonValue& root) noexcept
{
    return evaluatePointer(ctx.tokens, root);
}

} // namespace json_query::json_pointer::detail
