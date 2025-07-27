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

// Forward declaration for fanOut function used in TokenProcessingStrategy
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos);

using json_query::json_path::internal::ContainerCursor;
using json_query::json_path::internal::ResultCollector;
using internal::acquirePooledArray;
using internal::IterativeRecursiveDescent;

// ---------------------------------------------------------------------------
//  TableGen-Inspired Token Processing Architecture
// ---------------------------------------------------------------------------

// Enum for different token processing strategies
enum class TokenProcessingType {
    UnionDetection,           // Consecutive tokens that should be unioned
    BranchUniqueSelection,    // KeyList/Key after Recursive (branch-unique)
    StandardFanOut,           // Normal token evaluation with fan-out
    RootSelectorOnly          // Special case for root selector ($)
};

// Template for token processing strategy definitions
template<TokenProcessingType Type>
struct TokenProcessingDef {
    static constexpr bool enabled = false;
};

// Template specializations for each processing strategy
template<>
struct TokenProcessingDef<TokenProcessingType::UnionDetection> {
    static constexpr bool enabled = true;
    
    static bool matches(const PathEvalCtx& ctx, qsizetype i, const Token& tk, bool prevRecursive) {
        return (tk.kind == Token::Kind::Index || tk.kind == Token::Kind::Key || 
                tk.kind == Token::Kind::Filter || tk.kind == Token::Kind::Wildcard ||
                tk.kind == Token::Kind::Slice);
    }
};

template<>
struct TokenProcessingDef<TokenProcessingType::BranchUniqueSelection> {
    static constexpr bool enabled = true;
    
    static bool matches(const PathEvalCtx& ctx, qsizetype i, const Token& tk, bool prevRecursive) {
        return prevRecursive && (tk.kind == Token::Kind::KeyList || tk.kind == Token::Kind::Key);
    }
};

template<>
struct TokenProcessingDef<TokenProcessingType::StandardFanOut> {
    static constexpr bool enabled = true;
    
    static bool matches(const PathEvalCtx& ctx, qsizetype i, const Token& tk, bool prevRecursive) {
        return true; // Default fallback strategy
    }
};

template<>
struct TokenProcessingDef<TokenProcessingType::RootSelectorOnly> {
    static constexpr bool enabled = true;
    
    static bool matches(const PathEvalCtx& ctx, qsizetype i, const Token& tk, bool prevRecursive) {
        return ctx.tokens.size() == 1;
    }
};

// Template for token processing strategy implementations
template<TokenProcessingType Type>
struct TokenProcessingStrategy {
    static std::expected<QJsonArray, EvalError> process(
        const PathEvalCtx& ctx, qsizetype& i, const Token& tk, 
        const QJsonArray& working, const QJsonValue& root, bool& multi, bool prevRecursive) = delete;
};

// Specialization for Union Detection processing
template<>
struct TokenProcessingStrategy<TokenProcessingType::UnionDetection> {
    static std::expected<QJsonArray, EvalError> process(
        const PathEvalCtx& ctx, qsizetype& i, const Token& tk, 
        const QJsonArray& working, const QJsonValue& root, bool& multi, bool prevRecursive) {
        
        // Look ahead to see if we have consecutive tokens that should be unioned
        UnionDetectionResult unionDetectionResult = detectUnionTokens(ctx, i);
        
        if (unionDetectionResult.shouldUseUnion) {
            qDebug() << "[union] processing" << unionDetectionResult.unionTokens.size() << "consecutive selector tokens";
            
            auto result = processUnionTokens(ctx, unionDetectionResult.unionTokens, working, root);
            
            // Skip the tokens we just processed
            i = unionDetectionResult.nextIndex - 1;
            
            bool multiAfter = multi || addsMultiplicity(tk);
            if (result && result->isEmpty())
                return QJsonArray{}; // RFC 9535: empty result list when no matches

            multi = multiAfter;
            return result;
        }
        
        // If union detection doesn't apply, fall back to standard processing
        return std::unexpected(EvalError::TypeMismatchObject); // Signal fallback needed
    }
};

// Specialization for Branch-Unique Selection processing
template<>
struct TokenProcessingStrategy<TokenProcessingType::BranchUniqueSelection> {
    static std::expected<QJsonArray, EvalError> process(
        const PathEvalCtx& ctx, qsizetype& i, const Token& tk, 
        const QJsonArray& working, const QJsonValue& root, bool& multi, bool prevRecursive) {
        
        bool isLeaf = (i + 1 == ctx.tokens.size());
        
        auto result = processBranchUniqueSelection(ctx, i, working, root, isLeaf);
        
        if (result && result->isEmpty())
            return QJsonArray{}; // RFC 9535: empty result list when no matches

        return result;
    }
};

// Specialization for Standard Fan-Out processing
template<>
struct TokenProcessingStrategy<TokenProcessingType::StandardFanOut> {
    static std::expected<QJsonArray, EvalError> process(
        const PathEvalCtx& ctx, qsizetype& i, const Token& tk, 
        const QJsonArray& working, const QJsonValue& root, bool& multi, bool prevRecursive) {
        
        bool multiAfter = multi || addsMultiplicity(tk);
        
        // C++23 Monadic Chain - Elegant error composition for token evaluation!
        auto result = fanOut(ctx, tk, working, i)
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
                    // Use ArrayPool for deduplication array
                    auto pooledDedup = acquirePooledArray();
                    QJsonArray& dedup = *pooledDedup;
                    
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
                    return QJsonArray(dedup); // Return copy since pooled array will be returned to pool
                }
                return std::move(result);
            })
            .or_else([](EvalError error) -> std::expected<QJsonArray, EvalError> {
                qCDebug(jsonPathLog) << "evalStandard: fanOut returned error" << static_cast<int>(error);
                return std::unexpected(error);
            });
        
        multi = multiAfter;
        return result;
    }
};

// TableGen-inspired recursive dispatch table for token processing strategies
template<TokenProcessingType... Types>
struct TokenProcessingDispatchTable;

template<TokenProcessingType FirstType, TokenProcessingType... RestTypes>
struct TokenProcessingDispatchTable<FirstType, RestTypes...> {
    static std::expected<QJsonArray, EvalError> dispatch(
        const PathEvalCtx& ctx, qsizetype& i, const Token& tk, 
        const QJsonArray& working, const QJsonValue& root, bool& multi, bool prevRecursive) {
        
        if constexpr (TokenProcessingDef<FirstType>::enabled) {
            if (TokenProcessingDef<FirstType>::matches(ctx, i, tk, prevRecursive)) {
                auto result = TokenProcessingStrategy<FirstType>::process(ctx, i, tk, working, root, multi, prevRecursive);
                
                // Special handling for union detection fallback
                if constexpr (FirstType == TokenProcessingType::UnionDetection) {
                    if (!result) {
                        // Fall back to next strategy in the dispatch table
                        return TokenProcessingDispatchTable<RestTypes...>::dispatch(ctx, i, tk, working, root, multi, prevRecursive);
                    }
                }
                
                return result;
            }
        }
        
        // Try next strategy in the dispatch table
        return TokenProcessingDispatchTable<RestTypes...>::dispatch(ctx, i, tk, working, root, multi, prevRecursive);
    }
};

// Base case: no more strategies to try
template<>
struct TokenProcessingDispatchTable<> {
    static std::expected<QJsonArray, EvalError> dispatch(
        const PathEvalCtx& ctx, qsizetype& i, const Token& tk, 
        const QJsonArray& working, const QJsonValue& root, bool& multi, bool prevRecursive) {
        
        return std::unexpected(EvalError::TypeMismatchObject);
    }
};

// Compile-time dispatch table with prioritized strategy ordering
using TokenProcessingDispatcher = TokenProcessingDispatchTable<
    TokenProcessingType::UnionDetection,
    TokenProcessingType::BranchUniqueSelection,
    TokenProcessingType::StandardFanOut
>;

// ---------------------------------------------------------------------------
//  evalStandard – Refactored with TableGen-inspired architecture
// ---------------------------------------------------------------------------
std::expected<QJsonValue, EvalError> evalStandard(const PathEvalCtx& ctx, const QJsonValue& root)
{
    if (ctx.tokens.isEmpty())
        return QJsonValue(QJsonValue::Undefined);

    // Use ArrayPool for better memory management of working array
    auto pooledWorkingArray = acquirePooledArray();
    QJsonArray& workingArray = *pooledWorkingArray;
    workingArray.append(root);
    
    std::expected<QJsonArray, EvalError> working = QJsonArray(workingArray);
    bool multi = false;

    using json_query::json_path::internal::qt_hash;

    for (qsizetype i = 1; i < ctx.tokens.size() && working; ++i)
    {
        const Token& tk = ctx.tokens[i];
        qDebug() << "[stage] token" << i << ": kind=" << static_cast<int>(tk.kind)
                 << "working size=" << working->size();

        bool prevRecursive = (i>0 && ctx.tokens[i-1].kind == Token::Kind::Recursive);
        
        qCDebug(jsonPathLog) << "[evalStandard] Token" << i << "kind=" << static_cast<int>(tk.kind) << "prevRecursive=" << prevRecursive;

        // TableGen-inspired dispatch: Use compile-time strategy selection
        working = TokenProcessingDispatcher::dispatch(ctx, i, tk, *working, root, multi, prevRecursive);
        
        if (!working) {
            return std::unexpected(working.error());
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

// Direct array-based fan-out using TableGen dispatch
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos)
{
    // Use ArrayPool for better memory management
    auto pooledArray = acquirePooledArray();
    QJsonArray& result = *pooledArray;
    
    ResultCollector collector(&result);
    auto streamer = collector.getStreamer();
    
    // Use TableGen-inspired error handling dispatch directly
    internal::ErrorHandlingDispatcher::dispatch(tk, tokenPos, ctx, src, streamer);
    
    // Check if an error occurred during processing
    if (collector.hasError()) {
        return std::unexpected(collector.getLastError());
    }
    
    // Return a copy since pooled array will be returned to pool
    return QJsonArray(result);
}

} // namespace json_query::json_path::detail
