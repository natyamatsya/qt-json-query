// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/Common.h"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include <expected>
#include <QtCore/QJsonValue>
#include <utility>

namespace json_query::json_path::internal
{

/**
 * @brief Type-specialized token evaluators for eliminating runtime type checks
 *
 * This system provides compile-time specialization based on both Token::Kind
 * and QJsonValue::Type, eliminating the runtime type checking overhead that
 * occurs in every token evaluator (v.isObject(), v.isArray(), etc.).
 */

/**
 * @brief Primary template for typed token evaluation
 *
 * Provides compile-time dispatch based on both token kind and JSON value type,
 * eliminating runtime type checking and enabling aggressive compiler optimizations.
 */
template <Token::Kind TokenKind, QJsonValue::Type ValueType>
struct TypedTokenEvaluator
{
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
    {
        // Default implementation returns empty result for unsupported combinations
        return QJsonArray{};
    }
};

// =============================================================================
// Key Token Specializations
// =============================================================================

/**
 * @brief Key evaluation on Object - optimized path
 */
template <>
struct TypedTokenEvaluator<Token::Kind::Key, QJsonValue::Object>
{
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v) noexcept
    {
        // No type check needed - we know it's an object
        const auto obj{v.toObject()};
        const auto it{obj.find(tk.key)};
        if (it == obj.end())
            return QJsonArray{}; // Key not found

        // Use ArrayPool for result optimization
        auto        pooledArray{acquirePooledArray()};
        QJsonArray& result = *pooledArray;
        result.append(it.value());

        return std::move(result);
    }
};

/**
 * @brief Key evaluation on non-Object types - fast rejection
 */
template <QJsonValue::Type ValueType>
struct TypedTokenEvaluator<Token::Kind::Key, ValueType>
{
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& /*ctx*/, const Token& /*tk*/, const QJsonValue& /*v*/) noexcept
    {
        // Compile-time rejection for non-object types
        static_assert(ValueType != QJsonValue::Object, "Object specialization should be used");
        return QJsonArray{}; // Empty result for non-objects
    }
};

// =============================================================================
// Index Token Specializations
// =============================================================================

/**
 * @brief Index evaluation on Array - optimized path
 */
template <>
struct TypedTokenEvaluator<Token::Kind::Index, QJsonValue::Array>
{
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
    {
        // No type check needed - we know it's an array
        const auto arr{v.toArray()};
        const auto len{arr.size()};

        // Normalize negative indices
        auto normalizedIndex{tk.index};
        if (normalizedIndex < 0)
            normalizedIndex += len;

        // Bounds check
        if (normalizedIndex < 0 || normalizedIndex >= len)
            return QJsonArray{}; // Index out of bounds

        // Use ArrayPool for result optimization
        auto        pooledArray{acquirePooledArray()};
        QJsonArray& result = *pooledArray;
        result.append(arr[normalizedIndex]);

        return std::move(result);
    }
};

/**
 * @brief Index evaluation on non-Array types - fast rejection
 */
template <QJsonValue::Type ValueType>
struct TypedTokenEvaluator<Token::Kind::Index, ValueType>
{
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& /*ctx*/, const Token& /*tk*/, const QJsonValue& /*v*/) noexcept
    {
        // Compile-time rejection for non-array types
        static_assert(ValueType != QJsonValue::Array, "Array specialization should be used");
        return QJsonArray{}; // Empty result for non-arrays
    }
};

// =============================================================================
// Wildcard Token Specializations
// =============================================================================

/**
 * @brief Wildcard evaluation on Object - optimized path
 */
template <>
struct TypedTokenEvaluator<Token::Kind::Wildcard, QJsonValue::Object>
{
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& /*ctx*/, const Token& /*tk*/, const QJsonValue& v) noexcept
    {
        // No type check needed - we know it's an object
        const auto obj{v.toObject()};

        // Use ArrayPool for result optimization
        auto        pooledArray{acquirePooledArray()};
        QJsonArray& result = *pooledArray;

        // Collect all values from object
        for (auto it = obj.begin(); it != obj.end(); ++it)
            result.append(it.value());

        return std::move(result);
    }
};

/**
 * @brief Wildcard evaluation on Array - optimized path
 */
template <>
struct TypedTokenEvaluator<Token::Kind::Wildcard, QJsonValue::Array>
{
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& /*ctx*/, const Token& /*tk*/, const QJsonValue& v) noexcept
    {
        // No type check needed - we know it's an array
        const auto arr{v.toArray()};

        // Use ArrayPool for result optimization
        auto        pooledArray{acquirePooledArray()};
        QJsonArray& result = *pooledArray;

        // Collect all values from array
        for (const auto& item : arr)
            result.append(item);

        return std::move(result);
    }
};

/**
 * @brief Wildcard evaluation on other types - fast rejection
 */
template <QJsonValue::Type ValueType>
struct TypedTokenEvaluator<Token::Kind::Wildcard, ValueType>
{
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& /*ctx*/, const Token& /*tk*/, const QJsonValue& /*v*/) noexcept
    {
        // Compile-time rejection for non-container types
        static_assert(ValueType != QJsonValue::Object && ValueType != QJsonValue::Array,
                      "Container specializations should be used");
        return QJsonArray{}; // Empty result for non-containers
    }
};

// =============================================================================
// Type-Aware Dispatch System
// =============================================================================

namespace FastDispatch
{

/**
 * @brief Optimized Key token dispatch with forced inlining
 */
QT_QUERY_JSON_ALWAYS_INLINE static std::expected<QJsonArray, EvalError>
dispatchKey(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
{
    // Optimized for the most common case: Object keys
    if (v.isObject())
        return TypedTokenEvaluator<Token::Kind::Key, QJsonValue::Object>::eval(ctx, tk, v);
    // Fast path for empty results on non-objects
    return QJsonArray{};
}

/**
 * @brief Optimized Index token dispatch with forced inlining
 */
QT_QUERY_JSON_ALWAYS_INLINE static std::expected<QJsonArray, EvalError>
dispatchIndex(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
{
    // Optimized for the most common case: Array indices
    if (v.isArray())
        return TypedTokenEvaluator<Token::Kind::Index, QJsonValue::Array>::eval(ctx, tk, v);
    // Fast path for empty results on non-arrays
    return QJsonArray{};
}

/**
 * @brief Optimized Wildcard token dispatch with forced inlining
 */
QT_QUERY_JSON_ALWAYS_INLINE static std::expected<QJsonArray, EvalError>
dispatchWildcard(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
{
    // Handle both objects and arrays efficiently
    if (v.isObject())
        return TypedTokenEvaluator<Token::Kind::Wildcard, QJsonValue::Object>::eval(ctx, tk, v);
    else if (v.isArray())
        return TypedTokenEvaluator<Token::Kind::Wildcard, QJsonValue::Array>::eval(ctx, tk, v);
    // Fast path for empty results on other types
    return QJsonArray{};
}

} // namespace FastDispatch

/**
 * @brief Simplified type-aware dispatcher with reduced complexity
 *
 * This version focuses on the most common cases and uses fast dispatch
 * functions to reduce inline cost while maintaining performance.
 */
template <Token::Kind TokenKind>
class TypeAwareDispatcher
{
  public:
    [[gnu::always_inline]] static std::expected<QJsonArray, EvalError>
    dispatch(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
    {
        // Use specialized fast dispatch for common cases
        if constexpr (TokenKind == Token::Kind::Key)
        {
            return FastDispatch::dispatchKey(ctx, tk, v);
        }
        else if constexpr (TokenKind == Token::Kind::Index)
        {
            return FastDispatch::dispatchIndex(ctx, tk, v);
        }
        else if constexpr (TokenKind == Token::Kind::Wildcard)
        {
            return FastDispatch::dispatchWildcard(ctx, tk, v);
        }
        else
        {
            // Fallback for other token types - simplified dispatch
            return fallbackDispatch(ctx, tk, v);
        }
    }

  private:
    // Simplified fallback for less common token types
    static std::expected<QJsonArray, EvalError>
    fallbackDispatch(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
    {
        // Single runtime type check, then compile-time dispatch
        switch (v.type())
        {
        case QJsonValue::Object:
            return TypedTokenEvaluator<TokenKind, QJsonValue::Object>::eval(ctx, tk, v);
        case QJsonValue::Array:
            return TypedTokenEvaluator<TokenKind, QJsonValue::Array>::eval(ctx, tk, v);
        case QJsonValue::String:
            return TypedTokenEvaluator<TokenKind, QJsonValue::String>::eval(ctx, tk, v);
        case QJsonValue::Double:
            return TypedTokenEvaluator<TokenKind, QJsonValue::Double>::eval(ctx, tk, v);
        case QJsonValue::Bool:
            return TypedTokenEvaluator<TokenKind, QJsonValue::Bool>::eval(ctx, tk, v);
        case QJsonValue::Null:
            return TypedTokenEvaluator<TokenKind, QJsonValue::Null>::eval(ctx, tk, v);
        case QJsonValue::Undefined:
            return TypedTokenEvaluator<TokenKind, QJsonValue::Undefined>::eval(ctx, tk, v);
        default:
            return QJsonArray{}; // Unknown type
        }
    }
};

/**
 * @brief Optimized compile-time token dispatcher with reduced inline cost
 *
 * Split the complex dispatch logic into smaller, more inlinable functions
 * to address compiler inlining failures (cost 1250 vs threshold 225).
 */
class TypedCompileTimeTokenDispatcher
{
  public:
    QT_QUERY_JSON_ALWAYS_INLINE static std::expected<QJsonArray, EvalError>
    dispatch(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
    {
        // Use compile-time dispatch table for better optimization
        return dispatchTokenKind(ctx, tk, v, tk.kind);
    }

  private:
    // Separate dispatch function to reduce inline cost
    QT_QUERY_JSON_ALWAYS_INLINE static std::expected<QJsonArray, EvalError>
    dispatchTokenKind(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v, Token::Kind kind) noexcept
    {
        // Compile-time token dispatch with type-aware evaluation
        switch (kind)
        {
        case Token::Kind::Key:
            return FastDispatch::dispatchKey(ctx, tk, v);
        case Token::Kind::Index:
            return FastDispatch::dispatchIndex(ctx, tk, v);
        case Token::Kind::Wildcard:
            return FastDispatch::dispatchWildcard(ctx, tk, v);
        // Note: Other token types can be added as needed
        default:
            // Fallback to empty result for unsupported types
            return QJsonArray{};
        }
    }
};

} // namespace json_query::json_path::internal
