#pragma once

// This file contains inline implementations of critical hot path functions
// from JSONPathEvaluate.cpp to enable compiler inlining optimizations.
// Include this file at the end of the corresponding header file.

#include "json-query/Common.h"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"
#include "json-query/utils/SanitizerCompat.hpp"
#include "json-query/json-path/JSONPathTokenDispatch.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

namespace json_query::json_path::detail {

using internal::acquirePooledArray;
using internal::ResultCollector;

// =============================================================================
// Critical Hot Path Evaluation Functions - Inline Implementations
// =============================================================================

/**
 * @brief Inline convenience entry point for single value evaluation - critical hot path
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonValue, EvalError> evaluate(const PathEvalCtx& ctx, const QJsonValue& root)
{
    return evalStandard(ctx, root);
}

/**
 * @brief Inline array-based fan-out using TableGen dispatch - critical hot path
 *
 * This function is called frequently during token evaluation and is a prime
 * candidate for inlining to eliminate function call overhead.
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos)
{
    if (jsonPathLog().isDebugEnabled())
        qCDebug(jsonPathLog) << "DEBUG: fanOut called - tokenPos:" << tokenPos << "kind:" << static_cast<int>(tk.kind)
                            << "index:" << tk.index << "src.size():" << src.size();
    // Use ArrayPool for better memory management
    auto pooledArray = acquirePooledArray();
    QJsonArray& result = *pooledArray;

    ResultCollector collector(&result);
    auto streamer = collector.getStreamer();

    if (jsonPathLog().isDebugEnabled())
        qCDebug(jsonPathLog) << "DEBUG: fanOut - about to call ErrorHandlingDispatcher::dispatch";
    // Apply token per working element via ErrorHandlingDispatcher (correct serial semantics)
    internal::ErrorHandlingDispatcher::dispatch(tk, tokenPos, ctx, src, streamer);
    if (jsonPathLog().isDebugEnabled())
        qCDebug(jsonPathLog) << "DEBUG: fanOut - dispatch completed, result.size():" << result.size();

    // Check if an error occurred during processing
    if (QT_QUERY_JSON_UNLIKELY(collector.hasError())) {
        if (jsonPathLog().isDebugEnabled())
            qCDebug(jsonPathLog) << "DEBUG: fanOut - collector has error:" << static_cast<int>(collector.getLastError());
        return std::unexpected(collector.getLastError());
    }

    if (jsonPathLog().isDebugEnabled())
        qCDebug(jsonPathLog) << "DEBUG: fanOut - returning result with size:" << result.size();
    // Return by move to transfer ownership of the contents out of the pooled array
    // The pooled array instance will be returned to the pool in a moved-from (empty) state
    return std::move(result);
}

/**
 * @brief Inline convenience entry point for array evaluation - critical hot path
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> evaluateAll(const PathEvalCtx& ctx, const QJsonValue& root)
{
    // C++23 Monadic Chain - Elegant error composition without manual checks!
    return evaluate(ctx, root)
        .transform([&ctx](const QJsonValue& res) -> QJsonArray {
            // Special handling for root-only selectors: preserve the result as a single item
            // even if it's an array, to match RFC 9535 CTS expectations
            bool isRootSelectorOnly = (ctx.tokens.size() == 1);
            if (QT_QUERY_JSON_UNLIKELY(isRootSelectorOnly)) {
                // Root selector should return the document itself as a single result
                // even if it's an array, to match RFC 9535 CTS expectations
                if (res.isUndefined() || res.isNull()) return {};
                return QJsonArray{res};
            }

            // For non-root selectors, expand arrays into individual results
            if (QT_QUERY_JSON_LIKELY(res.isArray())) return res.toArray();
            if (res.isUndefined() || res.isNull()) return {};
            return QJsonArray{res};
        });
}

} // namespace json_query::json_path::detail
