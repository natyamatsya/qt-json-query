#pragma once

// This file contains inline implementations of critical hot path functions
// from JSONPathEvaluate.cpp to enable compiler inlining optimizations.
// Include this file at the end of the corresponding header file.

#include "json-query/Common.h"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/utils/SanitizerCompat.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

namespace json_query::json_path::detail {

using internal::acquirePooledArray;

// =============================================================================
// Critical Hot Path Evaluation Functions - Inline Implementations
// =============================================================================

/**
 * @brief Single-value evaluation: squash the node list + apply trailing function.
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonValue, DetailedEvalError> evaluateSingle(const PathEvalCtx& ctx, const QJsonValue& root)
{
    return evaluateTokenStream(ctx, root)
        .transform([&ctx](NodeList&& nl) -> QJsonValue {
            auto collapsed{squash(std::move(nl.nodes), nl.multi)};
            return applyTrailing(ctx.trailingFn, collapsed);
        });
}

/**
 * @brief Array evaluation: return the RFC 9535 node list directly.
 *
 * When a trailing function is present (e.g. `.length()`), the result is
 * collapsed first and then re-wrapped, since trailing functions reduce the
 * node list to a single value.
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, DetailedEvalError> evaluate(const PathEvalCtx& ctx, const QJsonValue& root)
{
    return evaluateTokenStream(ctx, root)
        .transform([&ctx](NodeList&& nl) -> QJsonArray {
            // Common case: no trailing function → return the node list as-is
            if (QT_QUERY_JSON_LIKELY(ctx.trailingFn == json_path::FunctionType::None))
                return std::move(nl.nodes);

            // Trailing function collapses the node list to a single value
            auto collapsed{squash(std::move(nl.nodes), nl.multi)};
            auto result{applyTrailing(ctx.trailingFn, collapsed)};
            if (result.isUndefined() || result.isNull()) return {};
            return QJsonArray{result};
        });
}

/**
 * @brief Dispatch a single token to a single value via switch over Token::Kind.
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v, qsizetype tokenPos)
{
    switch (tk.kind)
    {
    case Token::Kind::Key:       return eval<Token::Kind::Key>(ctx, tk, v, tokenPos);
    case Token::Kind::Index:     return eval<Token::Kind::Index>(ctx, tk, v, tokenPos);
    case Token::Kind::Slice:     return eval<Token::Kind::Slice>(ctx, tk, v, tokenPos);
    case Token::Kind::Wildcard:  return eval<Token::Kind::Wildcard>(ctx, tk, v, tokenPos);
    case Token::Kind::Recursive: return eval<Token::Kind::Recursive>(ctx, tk, v, tokenPos);
    case Token::Kind::Filter:    return eval<Token::Kind::Filter>(ctx, tk, v, tokenPos);
    case Token::Kind::KeyList:   return eval<Token::Kind::KeyList>(ctx, tk, v, tokenPos);
    }
    return std::unexpected(EvalError::TypeMismatchObject);
}

/**
 * @brief Apply a single token to every element in src, collecting results.
 *
 * RFC 9535 permissive semantics: selectors that don't match a value produce
 * empty results (not errors). Only collect successes.
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos)
{
    auto  pooledArray{acquirePooledArray()};
    auto& result = *pooledArray;

    for (const auto& v : src)
    {
        auto tokenResult{evaluateToken(ctx, tk, v, tokenPos)};
        if (tokenResult)
            for (const auto& item : *tokenResult)
                result.append(item);
    }

    return std::move(result);
}

} // namespace json_query::json_path::detail
