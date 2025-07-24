#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include "json-query/json-path/internal/ContextAwareContainerCursor.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"
#include <expected>
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathPointerConversion.hpp"
#include "json-query/json-path/internal/TokenDispatchTable.hpp"

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
using json_query::json_path::internal::ResultStreamer;
using json_query::json_path::internal::ResultCollector;
using internal::acquirePooledArray;
using internal::IterativeRecursiveDescent;

// ---------------------------------------------------------------------------
//  Basic helpers (free versions copied from legacy JSONPath.cpp)
// --------------------------------------------------------------
int normalizeIndex(int idx, int size)
{
    return idx < 0 ? size + idx : idx;
}

std::expected<QJsonArray, EvalError> evalSlice(const QJsonArray& array, const Slice& s)
{
    // Use pooled array to reduce allocations
    auto pooledArray = acquirePooledArray();
    QJsonArray& out = *pooledArray;

    const int len = array.size();
    constexpr qsizetype SENTINEL = std::numeric_limits<qsizetype>::max();

    // Step normalisation ----------------------------------------------------
    qsizetype step = s.step;
    if (step == 0)
        return QJsonArray{}; // Empty result for zero step (RFC 9535 compliance)

    qsizetype start = s.start;
    qsizetype stop  = s.end;

    const bool forward = step > 0;

    // ---------------- Python's PySlice_AdjustIndices ----------------------
    auto translate_default = [&](qsizetype &idx, bool isStart){
        if (idx != SENTINEL) return;
        if (forward)
            idx = isStart ? 0 : len;
        else
            idx = isStart ? (len - 1) : -1;
    };

    translate_default(start, /*isStart=*/true);
    translate_default(stop,  /*isStart=*/false);

    // Convert negatives, then clamp
    auto fix_index = [&](qsizetype &idx, bool isStart){
        if (idx < 0) idx += len;

        if (forward) {
            if (idx < 0) idx = 0;
            if (idx > len) idx = len;
        } else {
            if (idx < -1) idx = -1;
            if (idx >= len) idx = len - 1;
        }
    };

    fix_index(start, /*isStart=*/true);
    fix_index(stop , /*isStart=*/false);

    // Final CPython-style clamp on stop depending on step sign
    if (forward) {
        if (stop > len) stop = len;
    } else {
        if (stop < -1) stop = -1;
    }

    // If stop was omitted (SENTINEL) and we have negative step, Python sets stop = -1 after adjustments
    if (!forward && s.end == SENTINEL)
        stop = -1;

    qCDebug(jsonPathLog) << "evalSlice norm start="<<start<<" stop="<<stop<<" step="<<step;

    qCDebug(jsonPathLog) << "iterating " << (forward ? "forward" : "backward");

    // Iterate -------------------------------------------------------------
    if (forward) {
        qsizetype i = start;
        while (i < stop) {
            if (i >= 0 && i < len) {
                qCDebug(jsonPathLog) << "  visiting i="<<i;
                out.append(array[static_cast<int>(i)]);
            }
            // Prevent overflow when i + step would wrap
            if (step <= 0) break; // should not happen, guard
            if (i > std::numeric_limits<qsizetype>::max() - step)
                break;
            i += step;
        }
    } else {
        qsizetype i = start;
        while (i > stop) {
            if (i >= 0 && i < len) {
                qCDebug(jsonPathLog) << "  visiting i="<<i;
                out.append(array[static_cast<int>(i)]);
            }
            // Prevent underflow when i + step would wrap
            if (step >= 0) break; // should not happen, guard
            if (i < std::numeric_limits<qsizetype>::min() - step)
                break;
            i += step;
        }
    }

    qCDebug(jsonPathLog) << "evalSlice result size="<<out.size();

    // Move result to avoid copying
    QJsonArray finalResult = std::move(out);
    return finalResult;
}

// ---------------------------------------------------------------------------
//  Wildcard evaluation with ContainerCursor optimization
// ---------------------------------------------------------------------------
std::expected<QJsonArray, EvalError> __wildcardObjectImpl(const QJsonObject& obj)
{
    // Use pooled array to reduce allocations
    auto pooledArray = acquirePooledArray();
    QJsonArray& out = *pooledArray;
    
    // Use ContainerCursor for optimized, zero-copy iteration
    auto cursor = ContainerCursor::object(obj);
    for (const auto& value : cursor) {
        out.append(value);
    }
    
    // Move result to avoid copying
    QJsonArray finalResult = std::move(out);
    return finalResult;
}

void __evaluateRecursiveImplStreaming(const QJsonValue& value, const ResultStreamer& streamer)
{
    // Use iterative implementation to reduce call stack memory overhead
    auto result = IterativeRecursiveDescent::evaluateIterative(value, streamer);
    if (!result) {
        // Handle error - for streaming, we can't propagate errors easily
        // This maintains backward compatibility with the original streaming interface
        qWarning() << "Iterative recursive descent failed with error:" << static_cast<int>(result.error());
    }
}

std::expected<QJsonArray, EvalError> __evaluateRecursiveImpl(const QJsonValue& value)
{
    // Use iterative implementation with array pooling for better memory efficiency
    return IterativeRecursiveDescent::evaluateIterativeArray(value);
}

std::expected<QJsonArray, EvalError> wildcardObject(const QJsonObject& obj)
{
    return __wildcardObjectImpl(obj);
}

std::expected<QJsonArray, EvalError> wildcardArray(const QJsonArray& arr)
{
    // Use pooled array to reduce allocations
    auto pooledArray = acquirePooledArray();
    QJsonArray& out = *pooledArray;
    
    // Use ContainerCursor for optimized, zero-copy iteration
    auto cursor = ContainerCursor::array(arr);
    for (const auto& item : cursor) {
        out.append(item);
    }
    
    // Move result to avoid copying
    QJsonArray finalResult = std::move(out);
    return finalResult;
}

std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, int /*unused*/)
{
    return __evaluateRecursiveImpl(value);
}

// ---------------------------------------------------------------------------
//  Token dispatcher with std::expected error handling
//  Now using TableGen-inspired declarative dispatch system
// ---------------------------------------------------------------------------
std::expected<QJsonArray, EvalError> evaluateTokenExpected(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    // Use TableGen-inspired dispatch table instead of manual switch
    return json_query::json_path::internal::TokenDispatcher::dispatch(ctx, tk, v);
}

// ---------------------------------------------------------------------------
//  Token dispatcher (ex-JSONPath::evaluateToken)
// ---------------------------------------------------------------------------
std::expected<QJsonArray, EvalError> evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return evaluateTokenExpected(ctx, tk, v);
}

// ---------------------------------------------------------------------------
//  Streaming-optimized fan-out helper
// ---------------------------------------------------------------------------
void fanOutStreaming(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, 
                    const ResultStreamer& streamer, qsizetype tokenPos = -1)
{
    bool anySuccess = false;
    EvalError lastError;
    
    // Determine if we should use permissive or strict error handling
    bool usePermissiveErrorHandling = false;
    
    if (tk.kind == Token::Kind::Index) {
        if (tokenPos == 1) {
            // Direct array access: always use permissive error handling (RFC 9535)
            usePermissiveErrorHandling = true;
        } else if (tokenPos > 1) {
            // Check if we're in a recursive descent context by looking back through tokens
            bool inRecursiveContext = false;
            for (qsizetype i = tokenPos - 1; i >= 1; --i) {
                const Token& prevToken = ctx.tokens[i];
                if (prevToken.kind == Token::Kind::Recursive) {
                    // Found recursive descent in the token chain
                    inRecursiveContext = true;
                    break;
                } else if (prevToken.kind == Token::Kind::Key) {
                    // Property access breaks the recursive context
                    break;
                }
                // Index tokens continue the recursive context
            }
            
            if (inRecursiveContext) {
                // After recursive descent: use permissive error handling (RFC 9535)
                usePermissiveErrorHandling = true;
            } else {
                // After property chain: use strict error handling (UpstreamArrayIndexOOB)
                usePermissiveErrorHandling = false;
            }
        }
    }
    
    for (const auto& v : src) {
        auto seg = evaluateTokenExpected(ctx, tk, v);
        if (seg) {
            // Success: stream results directly without intermediate array
            anySuccess = true;
            qDebug() << "[fanOutStreaming] kind=" << static_cast<int>(tk.kind) << "srcType"
                     << v.type() << "seg size=" << seg->size();
            
            // Stream all results directly
            streamer.emitArray(*seg);
        } else {
            // Failure: record error
            lastError = seg.error();
            qDebug() << "[fanOutStreaming] kind=" << static_cast<int>(tk.kind) << "srcType"
                     << v.type() << "failed with error:" << static_cast<int>(lastError);
            
            // Context-aware error handling for Index tokens
            if (tk.kind == Token::Kind::Index && !usePermissiveErrorHandling) {
                // Strict error handling: propagate errors immediately (property chain access)
                streamer.handleError(lastError);
                return;
            }
            // Permissive error handling: continue processing (direct access or after recursive descent)
        }
    }
    
    // For permissive error handling, don't emit error if no results (RFC 9535 compliance)
    if (tk.kind == Token::Kind::Index && usePermissiveErrorHandling && !anySuccess) {
        // Empty result for RFC 9535 compliance - no emission needed
        return;
    }
    
    // Only emit error if ALL evaluations failed (non-permissive contexts)
    if (!anySuccess && streamer.canHandleErrors()) {
        streamer.handleError(lastError);
    }
}

// Legacy array-based fan-out (for backward compatibility)
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos)
{
    // Use pooled array for better memory efficiency
    auto pooledResult = acquirePooledArray();
    ResultCollector collector(*pooledResult);
    
    fanOutStreaming(ctx, tk, src, collector.getStreamer(), tokenPos);
    
    // Move result to avoid copying
    QJsonArray finalResult = std::move(*pooledResult);
    return finalResult;
}

// ---------------------------------------------------------------------------
//  Utility helpers reused by evalStandard
// ---------------------------------------------------------------------------
static bool addsMultiplicity(const Token& tk)
{
    using enum Token::Kind;
    return tk.kind != Key && tk.kind != KeyList && tk.kind != Index;
}

static QJsonValue squash(QJsonArray arr, bool multi)
{
    if (arr.isEmpty())
        return multi ? QJsonArray{} : QJsonValue(QJsonValue::Undefined);
    if (!multi && arr.size()==1) return arr.first();
    return arr;
}

static QJsonValue applyTrailing(json_path::FunctionType fn, const QJsonValue& v)
{
    using enum json_path::FunctionType;

    switch (fn) {
    case None:   return v;

    case Length:
        if (v.isArray())  return v.toArray().size();
        if (v.isObject()) return v.toObject().size();
        return 0;

    case Min:
    case Max:
        if (!v.isArray()) return QJsonValue(QJsonValue::Undefined);
        {
            const auto arr = v.toArray();
            bool first=true; double best=0.0;
            for (const auto& e : arr) {
                if (!e.isDouble()) continue;
                const double d = e.toDouble();
                if (first || (fn==Min ? d<best : d>best))
                    best = d, first = false;
            }
            return first ? QJsonValue(QJsonValue::Undefined)
                         : QJsonValue(best);
        }
    }
    std::unreachable();
}

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

        // Check for union semantics: consecutive tokens that should be evaluated together
        // This handles both same-kind unions (e.g., $[0,2]) and mixed-type unions (e.g., $[?@.a,1])
        // BUT NOT sequential property access (e.g., $.a.b.c)
        if (tk.kind == Token::Kind::Index || tk.kind == Token::Kind::Key || 
            tk.kind == Token::Kind::Filter || tk.kind == Token::Kind::Wildcard ||
            tk.kind == Token::Kind::Slice) {
            
            // Look ahead to see if we have consecutive tokens that should be unioned
            QVector<qsizetype> unionTokens;
            unionTokens.append(i);
            
            qsizetype j = i + 1;
            while (j < ctx.tokens.size()) {
                const Token& nextTk = ctx.tokens[j];
                // Include any selector token type that can appear in brackets
                if (nextTk.kind == Token::Kind::Index || nextTk.kind == Token::Kind::Key ||
                    nextTk.kind == Token::Kind::Filter || nextTk.kind == Token::Kind::Wildcard ||
                    nextTk.kind == Token::Kind::Slice) {
                    unionTokens.append(j);
                    ++j;
                } else {
                    break; // Stop at non-selector tokens (e.g., Recursive)
                }
            }
            
            // Updated union condition: Use bracket group metadata for accurate union vs sequential distinction
            // 
            // Key insight: Tokens from the same bracket expression should be unioned,
            // tokens from separate bracket expressions should be processed sequentially.
            // This is the compiler design approach using semantic annotations.
            
            bool shouldUseUnion = false;
            
            if (unionTokens.size() > 1) {
                // Check if all tokens are from the same bracket expression
                bool allFromSameBracket = true;
                int firstBracketGroupId = ctx.tokens[unionTokens[0]].bracketGroupId;
                
                // If first token is not from a bracket, this can't be a bracket union
                if (firstBracketGroupId <= 0) {
                    allFromSameBracket = false;
                } else {
                    // Check if all subsequent tokens have the same bracket group ID
                    for (qsizetype i = 1; i < unionTokens.size(); ++i) {
                        if (ctx.tokens[unionTokens[i]].bracketGroupId != firstBracketGroupId) {
                            allFromSameBracket = false;
                            break;
                        }
                    }
                }
                
                if (allFromSameBracket) {
                    // All tokens from same bracket expression → union evaluation
                    // Examples: $[0,2], $['a','d'], $..['a','d']
                    shouldUseUnion = true;
                } else {
                    // Tokens from different sources → sequential evaluation
                    // Examples: $.a.b (dot notation), $..[2][3] (separate brackets)
                    shouldUseUnion = false;
                }
            }
            
            if (shouldUseUnion) {
                qDebug() << "[union] processing" << unionTokens.size() << "consecutive selector tokens";
                
                std::expected<QJsonArray, EvalError> unionResult = QJsonArray{};
                bool anySuccess = false;
                EvalError lastError;
                
                // Use context-aware cursor for efficient union result collection
                auto workingCursor = json_query::json_path::internal::makeSimpleContextCursor(*working, root, root);
                QJsonArray collectedResults;
                
                for (qsizetype tokenIdx : unionTokens) {
                    const Token& unionTk = ctx.tokens[tokenIdx];
                    auto tokenResult = fanOut(ctx, unionTk, *working, tokenIdx);
                    if (tokenResult) {
                        // Success: collect results using context-aware cursor for efficient processing
                        anySuccess = true;
                        qDebug() << "[union] token" << tokenIdx << "kind" << static_cast<int>(unionTk.kind) 
                                 << "contributed" << tokenResult->size() << "results";
                        
                        // Use context-aware cursor for efficient result merging
                        if (!tokenResult->isEmpty()) {
                            auto resultCursor = json_query::json_path::internal::makeSimpleContextCursor(*tokenResult, root, root);
                            for (const auto& [result, context] : resultCursor) {
                                collectedResults.append(result);
                            }
                        }
                        
                        lastError = EvalError(); // Removed EvalError::Ok
                    } else {
                        qDebug() << "[union] token" << tokenIdx << "kind" << static_cast<int>(unionTk.kind) 
                                 << "failed with error" << static_cast<int>(tokenResult.error());
                        lastError = tokenResult.error();
                    }
                }
                
                if (anySuccess) {
                    // For union operations, preserve duplicates as per RFC 9535 semantics
                    // Only deduplicate when explicitly required (not for basic union selectors)
                    unionResult = collectedResults;
                    qDebug() << "[union] collected" << collectedResults.size() << "results (preserving duplicates for RFC 9535 compliance)";
                } else {
                    // All union tokens failed
                    unionResult = std::unexpected(lastError);
                }
                
                working = unionResult;
                qDebug() << "[union] combined result size:" << working->size();
                
                // Skip the tokens we just processed
                i = j - 1;
                
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
            QStringList keys;
            qsizetype j = i;
            if (tk.kind == Token::Kind::KeyList) {
                keys = tk.key.split(u'\n');
            } else { // single Key
                keys.append(tk.key);
            }
            j = i + 1;

            bool isLeaf = (j == ctx.tokens.size());

            std::expected<QJsonArray, EvalError> next;
            
            // Use context-aware cursor for efficient working array processing
            auto workingCursor = json_query::json_path::internal::makeSimpleContextCursor(*working, root, root);
            for (const auto& [v, context] : workingCursor) {
                if (!v.isObject()) continue;
                const QJsonObject obj = v.toObject();

                bool all=true;
                for (const QString& k : keys)
                    if (!obj.contains(k)) { all=false; break; }
                if (!all) continue;

                if (isLeaf) {
                    if (keys.size() > 1) {
                        // Multi-property leaf union ⇒ return parent only (Jayway)
                        next->append(v);
                    } else {
                        // Single-property leaf: return only the value, not the parent
                        next->append(obj.value(keys.first()));
                        // RFC 9535: Don't include parent containers for single-key descendant selectors
                        // Removed: if (obj.size() == 1) next->append(v);
                    }
                } else {
                    // Non-leaf: parent first, then properties for traversal
                    next->append(v);
                    
                    // Use context-aware cursor for efficient key processing
                    for (const QString& k : keys) {
                        QJsonValue keyValue = obj.value(k);
                        next->append(keyValue);
                    }
                }
            }

            if (next->isEmpty())
                return QJsonArray{}; // RFC 9535: empty result list when no matches

            working = next;

            // Deduplicate container nodes at leaf to avoid duplicates
            if (isLeaf) {
                QSet<uint> seen;
                QJsonArray dedup;
                
                // Use context-aware cursor for efficient deduplication processing
                auto workingDedupCursor = json_query::json_path::internal::makeSimpleContextCursor(*working, root, root);
                for (const auto& [v2, context] : workingDedupCursor) {
                    if (v2.isObject()) {
                        uint h = qt_hash(QJsonDocument(v2.toObject()).toJson());
                        if (seen.contains(h)) continue;
                        seen.insert(h);
                    } else if (v2.isArray()) {
                        uint h = qt_hash(QJsonDocument(v2.toArray()).toJson());
                        if (seen.contains(h)) continue;
                        seen.insert(h);
                    }
                    dedup.append(v2);
                }
                working = dedup;
            }

            // Skip over tokens we have just processed
            i = j - 1;
            continue;
        } else {
            working = fanOut(ctx, tk, *working, i);
            if (!working) {
                return std::unexpected(working.error());
            }
            bool multiAfter = multi || addsMultiplicity(tk);
            if (working->isEmpty())
                return QJsonArray{}; // RFC 9535: empty result list when no matches

            // Deduplicate containers after normal fan-out when preceded by Recursive
            if (prevRecursive) {
                QSet<uint> seen;
                QJsonArray dedup;
                for (const auto& v : *working) {
                    if (v.isObject()) {
                        uint h = qt_hash(QJsonDocument(v.toObject()).toJson());
                        if (seen.contains(h)) continue;
                        seen.insert(h);
                    } else if (v.isArray()) {
                        uint h = qt_hash(QJsonDocument(v.toArray()).toJson());
                        if (seen.contains(h)) continue;
                        seen.insert(h);
                    }
                    dedup.append(v);
                }
                working = dedup;
            }

            multi = multiAfter;
        }
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
    auto res = evaluate(ctx, root);
    if (!res) {
        return std::unexpected(res.error());
    }
    
    // Special handling for root-only selectors: preserve the result as a single item
    // even if it's an array, to match RFC 9535 CTS expectations
    bool isRootSelectorOnly = (ctx.tokens.size() == 1);
    if (isRootSelectorOnly) {
        // Root selector should return the document itself as a single result
        if (res->isUndefined() || res->isNull()) return {};
        return QJsonArray{*res};
    }
    
    // For non-root selectors, expand arrays into individual results
    if (res->isArray()) return res->toArray();
    if (res->isUndefined() || res->isNull()) return {};
    return QJsonArray{*res};
}

} // namespace json_query::json_path::detail

// ---------------------------------------------------------------------------
//  TableGen-inspired token dispatch function implementations
//  These must be in the json_query::json_path::internal namespace
// ---------------------------------------------------------------------------

namespace json_query::json_path::internal {

// Simple dispatch functions that call the actual evalExpected functions
std::expected<QJsonArray, EvalError> dispatchKey(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) {
    return json_query::json_path::detail::evalExpected<Token::Kind::Key>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchIndex(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) {
    return json_query::json_path::detail::evalExpected<Token::Kind::Index>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchSlice(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) {
    return json_query::json_path::detail::evalExpected<Token::Kind::Slice>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchWildcard(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) {
    return json_query::json_path::detail::evalExpected<Token::Kind::Wildcard>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchRecursive(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) {
    return json_query::json_path::detail::evalExpected<Token::Kind::Recursive>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchFilter(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) {
    return json_query::json_path::detail::evalExpected<Token::Kind::Filter>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchKeyList(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) {
    return json_query::json_path::detail::evalExpected<Token::Kind::KeyList>(ctx, tk, v);
}

} // namespace json_query::json_path::internal
