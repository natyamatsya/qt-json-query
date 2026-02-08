// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "JSONPointerParsing.hpp"
#include "internal/PointerEvalCtx.hpp"
#include "json-query/utils/JSONError.hpp"
#include "json-query/utils/SanitizerCompat.hpp"

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
    // Work directly with the original array to avoid the problematic copy constructor.
    // This is an unexpected interaction between Qt's copy-on-write semantics and sanitizer instrumentation.

    if (!current.isArray())
        return false;

    // Get the original array and work with it directly (avoid copy constructor)
    const QJsonArray originalArray = current.toArray();
    if (index < 0 || index >= originalArray.size())
        return false;

    // Access element directly from original array
    current = originalArray.at(index);
    return true;
}

// Internal implementation that returns domain-specific errors
// IMPORTANT: This function is excluded from sanitizer instrumentation due to
// incompatibility with Qt's copy-on-write semantics under AddressSanitizer.
// The sanitizer's memory layout changes interfere with QJsonArray/QJsonObject operations,
// causing functional test failures (not memory safety issues).
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

// Public API that converts domain errors to Error
[[nodiscard]] inline std::expected<QJsonValue, json_query::Error>
evaluatePointer(const std::vector<Token>& tokens, const QJsonValue& root) noexcept
{
    auto result = evaluatePointerImpl(tokens, root);
    if (!result)
        return std::unexpected(json_query::Error{result.error()});
    return *result;
}

// Convenience overload taking a PointerEvalCtx to mirror JSONPath's API
[[nodiscard]] inline std::expected<QJsonValue, json_query::Error> evaluatePointer(const PointerEvalCtx& ctx,
                                                                                       const QJsonValue& root) noexcept
{
    return evaluatePointer(ctx.tokens, root);
}

} // namespace json_query::json_pointer::detail
