#pragma once

// This file contains inline implementations of critical hot path functions
// from JSONPathTokenDispatch.cpp to enable compiler inlining optimizations.
// Include this file at the end of the corresponding header file.

#include "json-query/Common.h"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"

namespace json_query::json_path::internal {

using namespace json_query::json_path::detail;

// =============================================================================
// Critical Hot Path Token Dispatch Functions - Inline Implementations
// =============================================================================

/**
 * @brief Inline dispatch function for Key tokens - critical hot path
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> dispatchKey(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::Key>(ctx, tk, v);
}

/**
 * @brief Inline dispatch function for Index tokens - critical hot path
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> dispatchIndex(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    qDebug() << "DEBUG: dispatchIndex called - index:" << tk.index << "value type:" << v.type();
    auto result = json_query::json_path::detail::eval<Token::Kind::Index>(ctx, tk, v);
    qDebug() << "DEBUG: dispatchIndex result has_value:" << result.has_value();
    return result;
}

/**
 * @brief Inline dispatch function for Slice tokens - critical hot path
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> dispatchSlice(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    qDebug() << "DEBUG: dispatchSlice called - start:" << tk.slice.start << "end:" << tk.slice.end
             << "step:" << tk.slice.step << "value type:" << v.type();
    auto result = json_query::json_path::detail::eval<Token::Kind::Slice>(ctx, tk, v);
    qDebug() << "DEBUG: dispatchSlice result has_value:" << result.has_value();
    return result;
}

/**
 * @brief Inline dispatch function for Wildcard tokens - critical hot path
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> dispatchWildcard(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::Wildcard>(ctx, tk, v);
}

/**
 * @brief Inline dispatch function for Recursive tokens - critical hot path
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> dispatchRecursive(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::Recursive>(ctx, tk, v);
}

/**
 * @brief Inline dispatch function for Filter tokens - critical hot path
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> dispatchFilter(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::Filter>(ctx, tk, v);
}

/**
 * @brief Inline dispatch function for KeyList tokens - critical hot path
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> dispatchKeyList(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::KeyList>(ctx, tk, v);
}

} // namespace json_query::json_path::internal
