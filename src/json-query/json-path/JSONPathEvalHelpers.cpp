#include "json-query/json-path/JSONPathEvalHelpers.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathTokenDispatch.hpp"
#include "json-query/json-path/internal/ContextAwareContainerCursor.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>
#include <QSet>
#include <QDebug>
#include <algorithm>
#include <limits>

namespace json_query::json_path::detail {

using json_query::json_path::internal::ContainerCursor;
using internal::acquirePooledArray;

// ---------------------------------------------------------------------------
//  Basic evaluation helpers
// ---------------------------------------------------------------------------

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

bool addsMultiplicity(const Token& tk)
{
    switch (tk.kind) {
    case Token::Kind::Wildcard:
    case Token::Kind::Recursive:
    case Token::Kind::KeyList:
        return true;
    default:
        return false;
    }
}

QJsonValue squash(QJsonArray arr, bool multi)
{
    if (multi || arr.size() != 1)
        return arr;
    return arr[0];
}

QJsonValue applyTrailing(json_path::FunctionType fn, const QJsonValue& v)
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
//  Union detection and processing helpers
// ---------------------------------------------------------------------------

// Helper: Check if a token is a selector token that can appear in unions
bool isSelectorToken(const Token& token) {
    return token.kind == Token::Kind::Index || token.kind == Token::Kind::Key ||
           token.kind == Token::Kind::Filter || token.kind == Token::Kind::Wildcard ||
           token.kind == Token::Kind::Slice;
}

// Helper: Collect consecutive selector tokens starting from a given index
QVector<qsizetype> collectConsecutiveSelectorTokens(const PathEvalCtx& ctx, qsizetype startIndex) {
    QVector<qsizetype> unionTokens;
    unionTokens.append(startIndex);
    
    qsizetype j = startIndex + 1;
    while (j < ctx.tokens.size()) {
        const Token& nextTk = ctx.tokens[j];
        if (isSelectorToken(nextTk)) {
            unionTokens.append(j);
            ++j;
        } else {
            break; // Stop at non-selector tokens (e.g., Recursive)
        }
    }
    
    return unionTokens;
}

// Helper: Check if all tokens are from the same bracket group
bool areTokensFromSameBracketGroup(const PathEvalCtx& ctx, const QVector<qsizetype>& tokenIndices) {
    if (tokenIndices.size() <= 1) return false;
    
    int firstBracketGroupId = ctx.tokens[tokenIndices[0]].bracketGroupId;
    
    // If first token is not from a bracket, this can't be a bracket union
    if (firstBracketGroupId <= 0) {
        return false;
    }
    
    // Check if all subsequent tokens have the same bracket group ID
    for (qsizetype i = 1; i < tokenIndices.size(); ++i) {
        if (ctx.tokens[tokenIndices[i]].bracketGroupId != firstBracketGroupId) {
            return false;
        }
    }
    
    return true;
}

UnionDetectionResult detectUnionTokens(const PathEvalCtx& ctx, qsizetype startIndex) {
    QVector<qsizetype> unionTokens = collectConsecutiveSelectorTokens(ctx, startIndex);
    bool shouldUseUnion = areTokensFromSameBracketGroup(ctx, unionTokens);
    qsizetype nextIndex = startIndex + unionTokens.size();
    
    return {unionTokens, shouldUseUnion, nextIndex};
}

// Helper: Process a single union token and collect its results
TokenProcessingResult processSingleUnionToken(
    const PathEvalCtx& ctx, 
    qsizetype tokenIdx, 
    const QJsonArray& working, 
    const QJsonValue& root)
{
    qCDebug(jsonPathLog) << "[processSingleUnionToken] Processing token" << tokenIdx << "with" << working.size() << "working values";
    
    // Use pooled array for efficient result collection
    auto pooledArray = acquirePooledArray();
    QJsonArray& results = *pooledArray;
    
    // Process each working value using monadic pattern
    auto processWorkingValue = [&](const QJsonValue& workingValue) -> std::expected<QJsonArray, EvalError> {
        return evaluateTokenExpected(ctx, ctx.tokens[tokenIdx], workingValue);
    };
    
    // Aggregate results using monadic error handling
    auto aggregateResults = [&]() -> std::expected<QJsonArray, EvalError> {
        for (qsizetype i = 0; i < working.size(); ++i) {
            const auto& workingValue = working[i];
            qCDebug(jsonPathLog) << "[processSingleUnionToken] Processing working value" << i << "of type" << static_cast<int>(workingValue.type());
            
            // Use monadic chaining for token evaluation
            auto tokenResult = processWorkingValue(workingValue);
            if (!tokenResult) {
                qCDebug(jsonPathLog) << "[processSingleUnionToken] Token evaluation failed with error" << static_cast<int>(tokenResult.error());
                return std::unexpected(tokenResult.error());
            }
            
            qCDebug(jsonPathLog) << "[processSingleUnionToken] Token evaluation succeeded with" << tokenResult->size() << "results";
            // Append results from this token using range-based iteration
            for (const auto& value : *tokenResult) {
                results.append(value);
            }
        }
        return results;
    };
    
    // Convert std::expected result to TokenProcessingResult using monadic pattern
    return aggregateResults()
        .transform([&](const QJsonArray& successResults) -> TokenProcessingResult {
            qCDebug(jsonPathLog) << "[processSingleUnionToken] Completed with" << successResults.size() << "total results";
            return TokenProcessingResult{
                .success = true,
                .results = QJsonArray(successResults), // Copy from pooled array
                .error = EvalError::KeyNotFound // Unused for success case
            };
        })
        .or_else([](EvalError error) -> std::expected<TokenProcessingResult, EvalError> {
            return TokenProcessingResult{
                .success = false,
                .results = QJsonArray{},
                .error = error
            };
        })
        .value(); // Safe to call value() since or_else always returns a value
}

// Helper: Merge results from multiple tokens using simple concatenation
QJsonArray mergeTokenResults(const QVector<QJsonArray>& resultArrays, const QJsonValue& root) {
    QJsonArray collectedResults;
    
    // Simple concatenation approach - avoid complex cursor logic that may cause infinite loops
    for (const auto& results : resultArrays) {
        for (const auto& value : results) {
            collectedResults.append(value);
        }
    }
    
    return collectedResults;
}

std::expected<QJsonArray, EvalError> processUnionTokens(
    const PathEvalCtx& ctx, 
    const QVector<qsizetype>& unionTokens, 
    const QJsonArray& working, 
    const QJsonValue& root)
{
    qCDebug(jsonPathLog) << "[processUnionTokens] Starting with" << unionTokens.size() << "tokens";
    
    // Monadic approach: Transform union tokens into results using monadic chaining
    QVector<QJsonArray> resultArrays;
    resultArrays.reserve(unionTokens.size());
    
    // Use monadic pattern to process each token and collect successful results
    auto processToken = [&](qsizetype tokenIdx) -> std::optional<QJsonArray> {
        qCDebug(jsonPathLog) << "[processUnionTokens] Processing token" << tokenIdx;
        
        auto tokenResult = processSingleUnionToken(ctx, tokenIdx, working, root);
        if (tokenResult.success) {
            qCDebug(jsonPathLog) << "[processUnionTokens] Token" << tokenIdx << "succeeded with" << tokenResult.results.size() << "results";
            return tokenResult.results;
        } else {
            qCDebug(jsonPathLog) << "[processUnionTokens] Token" << tokenIdx << "failed with error" << static_cast<int>(tokenResult.error) << "- continuing with other tokens";
            return std::nullopt; // Convert failure to empty optional for union semantics
        }
    };
    
    // Collect successful results using monadic transformation
    EvalError lastError = EvalError::TypeMismatchObject;
    for (qsizetype tokenIdx : unionTokens) {
        if (auto result = processToken(tokenIdx)) {
            resultArrays.append(*result);
        } else {
            // Track last error for potential failure case
            auto tokenResult = processSingleUnionToken(ctx, tokenIdx, working, root);
            if (!tokenResult.success) {
                lastError = tokenResult.error;
            }
        }
    }
    
    // Monadic error handling: Only fail if ALL tokens failed
    if (resultArrays.isEmpty()) {
        qCDebug(jsonPathLog) << "[processUnionTokens] All tokens failed, returning last error" << static_cast<int>(lastError);
        return std::unexpected(lastError);
    }
    
    qCDebug(jsonPathLog) << "[processUnionTokens] Merging results from" << resultArrays.size() << "successful arrays";
    auto mergedResults = mergeTokenResults(resultArrays, root);
    qCDebug(jsonPathLog) << "[processUnionTokens] Merged to" << mergedResults.size() << "total results";
    return mergedResults;
}

// Helper: Collect keys from consecutive Key/KeyList tokens
KeyCollectionResult collectKeysFromTokens(const PathEvalCtx& ctx, qsizetype startIndex)
{
    QStringList keys;
    qsizetype i = startIndex;
    
    while (i < ctx.tokens.size()) {
        const Token& token = ctx.tokens[i];
        if (token.kind == Token::Kind::Key) {
            keys.append(token.key);
            ++i;
        } else if (token.kind == Token::Kind::KeyList) {
            keys.append(token.key.split(u'\n'));
            ++i;
        } else {
            break;
        }
    }
    
    return {keys, i};
}

// Helper: Check if an object contains all required keys
bool objectContainsAllKeys(const QJsonObject& obj, const QStringList& keys)
{
    for (const QString& key : keys) {
        if (!obj.contains(key)) {
            return false;
        }
    }
    return true;
}

// Helper: Process object for leaf selection
void processObjectForLeafSelection(const QJsonObject& obj, const QStringList& keys, const QJsonValue& v, QJsonArray* results)
{
    if (!objectContainsAllKeys(obj, keys)) {
        return;
    }
    
    if (keys.size() > 1) {
        // Multi-property leaf union ⇒ return parent only (Jayway)
        results->append(v);
    } else {
        // Single-property leaf: return only the value, not the parent
        results->append(obj.value(keys.first()));
    }
}

// Helper: Process object for non-leaf selection
void processObjectForNonLeafSelection(const QJsonObject& obj, const QStringList& keys, const QJsonValue& v, QJsonArray* results)
{
    for (const QString& key : keys) {
        if (obj.contains(key)) {
            results->append(obj[key]);
        }
    }
}

// Helper: Deduplicate JSON values using hash-based approach
QJsonArray deduplicateJsonValues(const QJsonArray& input, const QJsonValue& root) {
    QSet<uint> seen;
    QJsonArray dedup;
    
    auto workingDedupCursor = json_query::json_path::internal::makeSimpleContextCursor(input, root, root);
    for (const auto& [v2, context] : workingDedupCursor) {
        if (v2.isObject()) {
            uint h = qHash(QJsonDocument(v2.toObject()).toJson());
            if (seen.contains(h)) continue;
            seen.insert(h);
        } else if (v2.isArray()) {
            uint h = qHash(QJsonDocument(v2.toArray()).toJson());
            if (seen.contains(h)) continue;
            seen.insert(h);
        }
        dedup.append(v2);
    }
    
    return dedup;
}

std::expected<QJsonArray, EvalError> processBranchUniqueSelection(
    const PathEvalCtx& ctx,
    qsizetype& i,
    const QJsonArray& working,
    const QJsonValue& root,
    bool isLeaf)
{
    auto keyResult = collectKeysFromTokens(ctx, i);
    const QStringList& keys = keyResult.keys;
    i = keyResult.nextIndex;
    
    if (keys.isEmpty()) {
        return std::unexpected(EvalError::KeyNotFound);
    }
    
    auto pooledArray = acquirePooledArray();
    QJsonArray& results = *pooledArray;
    
    for (const auto& workingValue : working) {
        if (!workingValue.isObject()) continue;
        
        const QJsonObject obj = workingValue.toObject();
        
        if (isLeaf) {
            processObjectForLeafSelection(obj, keys, workingValue, &results);
        } else {
            processObjectForNonLeafSelection(obj, keys, workingValue, &results);
        }
    }
    
    // Deduplicate results
    return deduplicateJsonValues(results, root);
}

} // namespace json_query::json_path::detail
