#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPathEvalHelpers.hpp"
#include "json-query/json-path/JSONPathTokenDispatch.hpp"
#include "json-query/json-path/JSONPathWildcardRecursive.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include "json-query/json-path/internal/ContextAwareContainerCursor.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"
#include <expected>
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathPointerConversion.hpp"
#include "json-query/json-path/internal/PassPipeline.hpp"

#include <array>
#include <deque>
#include <QSet>
#include <QJsonDocument>
#include "json-query/json-path/internal/QtHash.hpp"

#include <QStringList>
#include <QString>
#include <QDebug>
#include <algorithm>
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/internal/IterativeRecursiveDescent.hpp"
#include "json-query/json-path/internal/ArenaAllocator.hpp"

namespace json_query::json_path::detail {

using json_query::json_path::internal::ContainerCursor;
using json_query::json_path::internal::ResultCollector;
using internal::acquirePooledArray;
using internal::IterativeRecursiveDescent;

// ---------------------------------------------------------------------------
//  evalStandard – pure variant
// ---------------------------------------------------------------------------
std::expected<QJsonValue, EvalError> evalStandard(const PathEvalCtx& ctx, const QJsonValue& root)
{
    if (ctx.tokens.isEmpty())
        return QJsonValue(QJsonValue::Undefined);

    std::expected<QJsonArray, EvalError> working = QJsonArray{root};
    bool multi = false;

    using json_query::json_path::internal::qt_hash;

    for (qsizetype i = 1; i < ctx.tokens.size() && working; ++i)
    {
        const Token& tk = ctx.tokens[i];
        qDebug() << "[stage] token" << i << ": kind=" << static_cast<int>(tk.kind)
                 << "working size=" << working->size();

        bool prevRecursive = (i>0 && ctx.tokens[i-1].kind == Token::Kind::Recursive);
        
        qCDebug(jsonPathLog) << "[evalStandard] Token" << i << "kind=" << static_cast<int>(tk.kind) << "prevRecursive=" << prevRecursive;

        // Check for union semantics: consecutive tokens that should be evaluated together
        // This handles both same-kind unions (e.g., $[0,2]) and mixed-type unions (e.g., $[?@.a,1])
        // BUT NOT sequential property access (e.g., $.a.b.c)
        if (tk.kind == Token::Kind::Index || tk.kind == Token::Kind::Key || 
            tk.kind == Token::Kind::Filter || tk.kind == Token::Kind::Wildcard ||
            tk.kind == Token::Kind::Slice) {
            
            // Look ahead to see if we have consecutive tokens that should be unioned
            UnionDetectionResult unionDetectionResult = detectUnionTokens(ctx, i);
            
            if (unionDetectionResult.shouldUseUnion) {
                qDebug() << "[union] processing" << unionDetectionResult.unionTokens.size() << "consecutive selector tokens";
                
                working = processUnionTokens(ctx, unionDetectionResult.unionTokens, *working, root);
                
                // Skip the tokens we just processed
                i = unionDetectionResult.nextIndex - 1;
                
                bool multiAfter = multi || addsMultiplicity(tk);
                if (working->isEmpty())
                    return QJsonArray{}; // RFC 9535: empty result list when no matches

                multi = multiAfter;
                continue;
            }
        }

        // Branch-unique selection handling only when a KeyList follows a Recursive token.
        // This covers expressions like $..['a','b'] where we must avoid returning each
        // property value separately. For ordinary single-key access like $..['a'].x we
        // fall through to normal fan-out.
        if (prevRecursive && (tk.kind == Token::Kind::KeyList || tk.kind == Token::Kind::Key)) {
            bool isLeaf = (i + 1 == ctx.tokens.size());

            working = processBranchUniqueSelection(ctx, i, *working, root, isLeaf);
            
            if (working->isEmpty())
                return QJsonArray{}; // RFC 9535: empty result list when no matches

            continue;
        } else {
            bool multiAfter = multi || addsMultiplicity(tk);
            // C++23 Monadic Chain - Elegant error composition for token evaluation!
            working = fanOut(ctx, tk, *working, i)
                .and_then([&](QJsonArray&& result) -> std::expected<QJsonArray, EvalError> {
                    if (result.isEmpty()) {
                        return QJsonArray{}; // RFC 9535: empty result list when no matches
                    }
                    return std::move(result);
                })
                .and_then([&](QJsonArray&& result) -> std::expected<QJsonArray, EvalError> {
                    // Deduplicate containers after normal fan-out when preceded by Recursive
                    if (prevRecursive) {
                        QSet<uint> seen;
                        QJsonArray dedup;
                        for (const auto& v : result) {
                            if (v.isObject()) {
                                uint h = qHash(QJsonDocument(v.toObject()).toJson());
                                if (seen.contains(h)) continue;
                                seen.insert(h);
                            } else if (v.isArray()) {
                                uint h = qHash(QJsonDocument(v.toArray()).toJson());
                                if (seen.contains(h)) continue;
                                seen.insert(h);
                            }
                            dedup.append(v);
                        }
                        return std::move(dedup);
                    }
                    return std::move(result);
                })
                .or_else([](EvalError error) -> std::expected<QJsonArray, EvalError> {
                    qCDebug(jsonPathLog) << "evalStandard: fanOut returned error" << static_cast<int>(error);
                    return std::unexpected(error);
                });
            
            if (!working) {
                return std::unexpected(working.error());
            }
            
            multi = multiAfter;
        }

        if (working->isEmpty())
            return QJsonArray{}; // RFC 9535: empty result list when no matches
    }

    // Special case: for root selector ($), we should return the root document itself
    // not wrapped in an array, because the root selector should return the complete document
    // as a single result
    bool isRootSelectorOnly = (ctx.tokens.size() == 1);
    if (isRootSelectorOnly) {
        // Return the first (and only) element from the working array, which is the root document
        if (!working->isEmpty()) {
            return working->first();
        }
        return QJsonValue(QJsonValue::Undefined);
    }

    QJsonValue collapsed = squash(*std::move(working), multi);
    if (collapsed.isUndefined())
        return QJsonArray{}; // RFC 9535: no matches 

    return applyTrailing(ctx.trailingFn, collapsed);
}

// ---------------------------------------------------------------------------
//  Convenience entry points (pure)
// ---------------------------------------------------------------------------
std::expected<QJsonValue, EvalError> evaluate(const PathEvalCtx& ctx, const QJsonValue& root)
{
    return evalStandard(ctx, root);
}

std::expected<QJsonArray, EvalError> evaluateAll(const PathEvalCtx& ctx, const QJsonValue& root)
{
    // C++23 Monadic Chain - Elegant error composition without manual checks!
    return evaluate(ctx, root)
        .transform([&ctx](const QJsonValue& res) -> QJsonArray {
            // Special handling for root-only selectors: preserve the result as a single item
            // even if it's an array, to match RFC 9535 CTS expectations
            bool isRootSelectorOnly = (ctx.tokens.size() == 1);
            if (isRootSelectorOnly) {
                // Root selector should return the document itself as a single result
                if (res.isUndefined() || res.isNull()) return {};
                return QJsonArray{res};
            }
            
            // For non-root selectors, expand arrays into individual results
            if (res.isArray()) return res.toArray();
            if (res.isUndefined() || res.isNull()) return {};
            return QJsonArray{res};
        });
}

// Legacy array-based fan-out (for backward compatibility)
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos)
{
    // Use regular QJsonArray with pointer-based ResultCollector for memory safety
    QJsonArray result;
    ResultCollector collector(&result);
    
    // Use zero-overhead concept-based streaming
    auto conceptStreamer = collector.getConceptStreamer();
    
    // Explicitly call the template version to ensure proper error handling
    fanOutStreaming<decltype(conceptStreamer)>(ctx, tk, src, conceptStreamer, tokenPos);
    
    // Check if an error occurred during streaming
    if (collector.hasError()) {
        return std::unexpected(collector.getLastError());
    }
    
    // Return result
    return result;
}

} // namespace json_query::json_path::detail
