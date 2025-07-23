#include "json-query/json-path/JSONPathEvaluate.hpp"  // normalizeIndex, evalSlice
#include <expected>
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathPointerConversion.hpp"

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

namespace json_query::json_path::detail {

// --------------------------------------------------------------
// Basic helpers (free versions copied from legacy JSONPath.cpp)
// --------------------------------------------------------------
int normalizeIndex(int idx, int size)
{
    return idx < 0 ? size + idx : idx;
}

std::expected<QJsonArray, EvalError> evalSlice(const QJsonArray& array, const Slice& s)
{
    QJsonArray out;

    const int len = array.size();
    constexpr qsizetype SENTINEL = std::numeric_limits<qsizetype>::max();

    // Step normalisation ----------------------------------------------------
    qsizetype step = s.step;
    if (step == 0)
        return std::unexpected(EvalError::InvalidSlice); // zero-step ⇒ empty result per RFC & Python

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

    return out;
}

// ---------------------------------------------------------------------------
//  Wildcard and recursive helpers (moved from JSONPathEvaluate.cpp)
// ---------------------------------------------------------------------------
namespace {

std::expected<QJsonArray, EvalError> __wildcardObjectImpl(const QJsonObject& obj)
{
    QJsonArray out;
    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
        out.append(it.value());
    return out;
}

std::expected<QJsonArray, EvalError> __evaluateRecursiveImpl(const QJsonValue& value)
{
    QJsonArray out;
    
    // Add the current value itself
    out.append(value);
    
    // Recursively add all descendants
    if (value.isObject()) {
        const QJsonObject obj = value.toObject();
        for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
            auto subResult = __evaluateRecursiveImpl(it.value());
            if (!subResult) {
                return std::unexpected(subResult.error());
            }
            for (const auto& item : *subResult) {
                out.append(item);
            }
        }
    } else if (value.isArray()) {
        const QJsonArray arr = value.toArray();
        for (const auto& item : arr) {
            auto subResult = __evaluateRecursiveImpl(item);
            if (!subResult) {
                return std::unexpected(subResult.error());
            }
            for (const auto& subItem : *subResult) {
                out.append(subItem);
            }
        }
    }
    
    return out;
}

} // anonymous

std::expected<QJsonArray, EvalError> wildcardObject(const QJsonObject& obj)
{
    return __wildcardObjectImpl(obj);
}

std::expected<QJsonArray, EvalError> wildcardArray(const QJsonArray& arr)
{
    QJsonArray out;
    for (const auto& item : arr)
        out.append(item);
    return out;
}

std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, int /*unused*/)
{
    return __evaluateRecursiveImpl(value);
}

} // namespace json_query::json_path::detail

namespace json_query::json_path::detail {

// ---------------------------------------------------------------------------
//  Token dispatcher with std::expected error handling
// ---------------------------------------------------------------------------
std::expected<QJsonArray, EvalError> evaluateTokenExpected(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    using enum Token::Kind;
    switch (tk.kind) {
        case Key:       return evalExpected<Key>(ctx, tk, v);
        case Index:     return evalExpected<Index>(ctx, tk, v);
        case Slice:     return evalExpected<Slice>(ctx, tk, v);
        case Wildcard:  return evalExpected<Wildcard>(ctx, tk, v);
        case Recursive: return evalExpected<Recursive>(ctx, tk, v);
        case Filter:    return evalExpected<Filter>(ctx, tk, v);
        case KeyList:   return evalExpected<KeyList>(ctx, tk, v);
        default:        return std::unexpected(EvalError::TypeMismatchObject);
    }
}

// ---------------------------------------------------------------------------
//  Token dispatcher (ex-JSONPath::evaluateToken)
// ---------------------------------------------------------------------------
std::expected<QJsonArray, EvalError> evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return evaluateTokenExpected(ctx, tk, v);
}

// ---------------------------------------------------------------------------
//  Fan-out helper (adapted from legacy implementation)
// ---------------------------------------------------------------------------
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src)
{
    QJsonArray dst;
    bool anySuccess = false;
    EvalError lastError = EvalError::TypeMismatchObject;
    
    for (const auto& v : src) {
        auto seg = evaluateTokenExpected(ctx, tk, v);
        if (seg) {
            // Success: collect results
            anySuccess = true;
            qDebug() << "[fanOut] kind=" << static_cast<int>(tk.kind) << "srcType"
                     << v.type() << "seg size=" << seg->size();
            for (const auto& e : *seg)
                dst.append(e);
        } else {
            // Failure: record error but continue processing other values
            lastError = seg.error();
            qDebug() << "[fanOut] kind=" << static_cast<int>(tk.kind) << "srcType"
                     << v.type() << "failed with error:" << static_cast<int>(lastError);
        }
    }
    
    // Only fail if ALL evaluations failed
    if (!anySuccess) {
        return std::unexpected(lastError);
    }
    
    return dst;
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
                EvalError lastError = EvalError::TypeMismatchObject;
                
                for (qsizetype tokenIdx : unionTokens) {
                    const Token& unionTk = ctx.tokens[tokenIdx];
                    auto tokenResult = fanOut(ctx, unionTk, *working);
                    if (tokenResult) {
                        // Success: collect results
                        anySuccess = true;
                        qDebug() << "[union] token" << tokenIdx << "kind" << static_cast<int>(unionTk.kind) 
                                 << "produced" << tokenResult->size() << "results";
                        
                        // Combine results (union semantics)
                        for (const auto& result : *tokenResult) {
                            unionResult->append(result);
                        }
                    } else {
                        // Failure: record error but continue processing other selectors
                        lastError = tokenResult.error();
                        qDebug() << "[union] token" << tokenIdx << "kind" << static_cast<int>(unionTk.kind) 
                                 << "failed with error:" << static_cast<int>(lastError);
                    }
                }
                
                // Only fail if ALL selectors failed
                if (!anySuccess) {
                    return std::unexpected(lastError);
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
            for (const auto& v : *working) {
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
                    for (const QString& k : keys)
                        next->append(obj.value(k));
                }
            }

            if (next->isEmpty())
                return QJsonArray{}; // RFC 9535: empty result list when no matches

            working = next;

            // Deduplicate container nodes at leaf to avoid duplicates
            if (isLeaf) {
                QSet<uint> seen;
                QJsonArray dedup;
                for (const auto& v2 : *working) {
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
            working = fanOut(ctx, tk, *working);
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

    // Special case: for root selector ($), we should return the working array directly
    // instead of squashing it, because the root selector should return the complete document
    // as a single result, not unwrap array contents
    bool isRootSelectorOnly = (ctx.tokens.size() == 1);
    if (isRootSelectorOnly) {
        return *working;
    }

    QJsonValue collapsed = squash(*std::move(working), multi);
    if (collapsed.isUndefined())
        return QJsonArray{}; // RFC 9535: no matches 

    return applyTrailing(ctx.trailingFn, collapsed);
}

// ---------------------------------------------------------------------------
//  Enhanced evaluation with std::expected error handling
// ---------------------------------------------------------------------------
std::expected<QJsonValue, EvalError> evaluateWithErrorHandling(const PathEvalCtx& ctx, const QJsonValue& root)
{
    if (ctx.tokens.isEmpty())
        return QJsonValue(QJsonValue::Undefined);

    std::expected<QJsonValue, EvalError> current = root;
    
    // Skip leading root token ('$' or '@') if present
    int startIdx = 0;
    if (!ctx.tokens.isEmpty() && ctx.tokens.front().kind == Token::Kind::Key) {
        const QString& k = ctx.tokens.front().key;
        if (k == u"$" || k == u"@") {
            startIdx = 1;
        }
    }

    // Process tokens sequentially with error handling
    for (int i = startIdx; i < ctx.tokens.size(); ++i) {
        const Token& tk = ctx.tokens[i];
        
        // Use error-handling evaluation for Index tokens
        if (tk.kind == Token::Kind::Index) {
            auto result = evalExpected<Token::Kind::Index>(ctx, tk, *current);
            if (!result) {
                return std::unexpected(result.error());
            }
            
            // Convert QJsonArray result back to single value
            QJsonArray arr = *result;
            if (arr.isEmpty()) {
                return QJsonValue(QJsonValue::Undefined);
            }
            current = arr.first();
        }
        // Use legacy evaluation for other token types
        else {
            auto results = evaluateTokenExpected(ctx, tk, *current);
            if (!results) {
                return std::unexpected(results.error());
            }
            if (results->isEmpty()) {
                return QJsonValue(QJsonValue::Undefined);
            }
            current = results->first();
        }
    }
    
    return *current;
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
    if (res->isArray()) return res->toArray();
    if (res->isUndefined() || res->isNull()) return {};
    return QJsonArray{*res};
}

} // namespace json_query::json_path::detail
