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
std::expected<QJsonValue, DetailedEvalError> evaluate(const PathEvalCtx& ctx, const QJsonValue& root)
{
    return evaluateTokenStream(ctx, root);
}

/**
 * @brief Apply a single token to every element in src, collecting results.
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos)
{
    auto pooledArray{acquirePooledArray()};
    auto& result = *pooledArray;

    ResultCollector collector{&result};
    auto streamer{collector.getStreamer()};

    internal::ErrorHandlingDispatcher::dispatch(tk, tokenPos, ctx, src, streamer);

    if (QT_QUERY_JSON_UNLIKELY(collector.hasError()))
        return std::unexpected(collector.getLastError());

    qCDebug(jsonPathLog) << "fanOut: tokenPos=" << tokenPos << "kind=" << static_cast<int>(tk.kind)
                         << "results=" << result.size();
    return std::move(result);
}

/**
 * @brief Inline convenience entry point for array evaluation - critical hot path
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonArray, DetailedEvalError> evaluateAll(const PathEvalCtx& ctx, const QJsonValue& root)
{
    return evaluate(ctx, root)
        .transform([&ctx](const QJsonValue& res) -> QJsonArray {
            // Root-only selector ($): wrap the document as a single result
            const auto isRootOnly{ctx.tokens.size() == 1};
            if (QT_QUERY_JSON_UNLIKELY(isRootOnly))
            {
                if (res.isUndefined() || res.isNull()) return {};
                return QJsonArray{res};
            }

            // Non-root selectors: expand arrays into individual results
            if (QT_QUERY_JSON_LIKELY(res.isArray())) return res.toArray();
            if (res.isUndefined() || res.isNull()) return {};
            return QJsonArray{res};
        });
}

} // namespace json_query::json_path::detail
