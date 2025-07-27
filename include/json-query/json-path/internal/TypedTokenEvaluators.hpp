#pragma once

#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include <expected>
#include <QtCore/QJsonValue>

namespace json_query::json_path::internal {

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
template<Token::Kind TokenKind, QJsonValue::Type ValueType>
struct TypedTokenEvaluator {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& ctx, 
        const Token& tk, 
        const QJsonValue& v) noexcept 
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
template<>
struct TypedTokenEvaluator<Token::Kind::Key, QJsonValue::Object> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/, 
        const Token& tk, 
        const QJsonValue& v) noexcept 
    {
        // No type check needed - we know it's an object
        const QJsonObject obj = v.toObject();
        const auto it = obj.find(tk.key);
        if (it == obj.end()) {
            return QJsonArray{}; // Key not found
        }
        
        // Use ArrayPool for result optimization
        auto pooledArray = acquirePooledArray();
        QJsonArray& result = *pooledArray;
        result.append(it.value());
        
        return QJsonArray(result);
    }
};

/**
 * @brief Key evaluation on non-Object types - fast rejection
 */
template<QJsonValue::Type ValueType>
struct TypedTokenEvaluator<Token::Kind::Key, ValueType> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/, 
        const Token& /*tk*/, 
        const QJsonValue& /*v*/) noexcept 
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
template<>
struct TypedTokenEvaluator<Token::Kind::Index, QJsonValue::Array> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& ctx, 
        const Token& tk, 
        const QJsonValue& v) noexcept 
    {
        // No type check needed - we know it's an array
        const QJsonArray arr = v.toArray();
        const int len = arr.size();
        
        // Normalize negative indices
        int normalizedIndex = tk.index;
        if (normalizedIndex < 0) {
            normalizedIndex += len;
        }
        
        // Bounds check
        if (normalizedIndex < 0 || normalizedIndex >= len) {
            return QJsonArray{}; // Index out of bounds
        }
        
        // Use ArrayPool for result optimization
        auto pooledArray = acquirePooledArray();
        QJsonArray& result = *pooledArray;
        result.append(arr[normalizedIndex]);
        
        return QJsonArray(result);
    }
};

/**
 * @brief Index evaluation on non-Array types - fast rejection
 */
template<QJsonValue::Type ValueType>
struct TypedTokenEvaluator<Token::Kind::Index, ValueType> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/, 
        const Token& /*tk*/, 
        const QJsonValue& /*v*/) noexcept 
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
template<>
struct TypedTokenEvaluator<Token::Kind::Wildcard, QJsonValue::Object> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/, 
        const Token& /*tk*/, 
        const QJsonValue& v) noexcept 
    {
        // No type check needed - we know it's an object
        const QJsonObject obj = v.toObject();
        
        // Use ArrayPool for result optimization
        auto pooledArray = acquirePooledArray();
        QJsonArray& result = *pooledArray;
        
        // Collect all values from object
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            result.append(it.value());
        }
        
        return QJsonArray(result);
    }
};

/**
 * @brief Wildcard evaluation on Array - optimized path
 */
template<>
struct TypedTokenEvaluator<Token::Kind::Wildcard, QJsonValue::Array> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/, 
        const Token& /*tk*/, 
        const QJsonValue& v) noexcept 
    {
        // No type check needed - we know it's an array
        const QJsonArray arr = v.toArray();
        
        // Use ArrayPool for result optimization
        auto pooledArray = acquirePooledArray();
        QJsonArray& result = *pooledArray;
        
        // Collect all values from array
        for (const auto& item : arr) {
            result.append(item);
        }
        
        return QJsonArray(result);
    }
};

/**
 * @brief Wildcard evaluation on other types - fast rejection
 */
template<QJsonValue::Type ValueType>
struct TypedTokenEvaluator<Token::Kind::Wildcard, ValueType> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/, 
        const Token& /*tk*/, 
        const QJsonValue& /*v*/) noexcept 
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

/**
 * @brief Runtime type detection with compile-time dispatch
 * 
 * Detects the JSON value type at runtime and dispatches to the appropriate
 * compile-time specialized evaluator, eliminating type checks in the evaluator.
 */
template<Token::Kind TokenKind>
class TypeAwareDispatcher {
public:
    static std::expected<QJsonArray, EvalError> dispatch(
        const detail::PathEvalCtx& ctx, 
        const Token& tk, 
        const QJsonValue& v) noexcept 
    {
        // Single runtime type check, then compile-time dispatch
        switch (v.type()) {
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
 * @brief Enhanced compile-time token dispatcher with type specialization
 * 
 * Combines token dispatch elimination with type specialization for maximum
 * compile-time optimization and minimal runtime overhead.
 */
class TypedCompileTimeTokenDispatcher {
public:
    static inline std::expected<QJsonArray, EvalError> dispatch(
        const detail::PathEvalCtx& ctx, 
        const Token& tk, 
        const QJsonValue& v) noexcept 
    {
        // Compile-time token dispatch with type-aware evaluation
        switch (tk.kind) {
            case Token::Kind::Key:
                return TypeAwareDispatcher<Token::Kind::Key>::dispatch(ctx, tk, v);
            case Token::Kind::Index:
                return TypeAwareDispatcher<Token::Kind::Index>::dispatch(ctx, tk, v);
            case Token::Kind::Wildcard:
                return TypeAwareDispatcher<Token::Kind::Wildcard>::dispatch(ctx, tk, v);
            // Note: Other token types (Slice, Filter, etc.) can be added as needed
            default:
                // Fallback to original dispatch for unsupported types
                return std::unexpected(EvalError::TypeMismatchObject);
        }
    }
};

} // namespace json_query::json_path::internal
