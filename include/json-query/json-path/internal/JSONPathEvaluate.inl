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
 * @brief Single-value evaluation: squash the node list + apply trailing function.
 */
QT_QUERY_JSON_ALWAYS_INLINE
std::expected<QJsonValue, DetailedEvalError> evaluate(const PathEvalCtx& ctx, const QJsonValue& root)
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
std::expected<QJsonArray, DetailedEvalError> evaluateAll(const PathEvalCtx& ctx, const QJsonValue& root)
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

} // namespace json_query::json_path::detail
