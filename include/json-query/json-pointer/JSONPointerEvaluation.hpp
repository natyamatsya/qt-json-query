// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "JSONPointerParsing.hpp"
#include "internal/PointerEvalCtx.hpp"
#include "json-query/utils/JSONError.hpp"

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
    if (!current.isArray())
        return false;

    // Copy-init, not brace-init: QJsonArray{...} would select the
    // initializer_list constructor and wrap the array (see ADR-001)
    const QJsonArray arr = current.toArray();
    if (index < 0 || index >= arr.size())
        return false;

    current = arr.at(index);
    return true;
}

// Internal error carrying both the error code and the failing token index
struct DetailedEvalError
{
    EvalError     error{};
    std::uint16_t tokenIndex{};
};

// Internal implementation that returns domain-specific errors with token index
[[nodiscard]] inline std::expected<QJsonValue, DetailedEvalError> evaluatePointerImpl(const std::vector<Token>& tokens,
                                                                                      const QJsonValue& root) noexcept
{
    if (tokens.empty())
        return root; // success with root value

    QJsonValue current{root};
    for (std::uint16_t i{0}; i < tokens.size(); ++i)
    {
        const auto& tk{tokens[i]};
        switch (current.type())
        {
        case QJsonValue::Object:
            if (tk.kind != Token::Kind::Key)
                return std::unexpected(DetailedEvalError{EvalError::TypeMismatchObject, i});
            if (!stepObject(current, tk.key))
                return std::unexpected(DetailedEvalError{EvalError::KeyNotFound, i});
            break;
        case QJsonValue::Array:
            if (tk.kind != Token::Kind::Index)
                return std::unexpected(DetailedEvalError{EvalError::TypeMismatchArray, i});
            if (!stepArray(current, tk.index))
                return std::unexpected(DetailedEvalError{EvalError::IndexOutOfRange, i});
            break;
        default:
            return std::unexpected(DetailedEvalError{EvalError::TypeMismatchObject, i});
        }
    }
    return current; // success
}

// Public API that converts domain errors to Error (with token index in detail)
[[nodiscard]] inline std::expected<QJsonValue, json_query::Error> evaluatePointer(const std::vector<Token>& tokens,
                                                                                  const QJsonValue& root) noexcept
{
    auto result{evaluatePointerImpl(tokens, root)};
    if (!result)
        return std::unexpected(json_query::Error{result.error().error, result.error().tokenIndex});
    return *result;
}

// Convenience overload taking a PointerEvalCtx to mirror JSONPath's API
[[nodiscard]] inline std::expected<QJsonValue, json_query::Error> evaluatePointer(const PointerEvalCtx& ctx,
                                                                                  const QJsonValue&     root) noexcept
{
    return evaluatePointer(ctx.tokens, root);
}

} // namespace json_query::json_pointer::detail
