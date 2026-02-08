// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPathEvalHelpers.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include "json-query/json-path/JSONPathError.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPathWildcardRecursive.hpp"
#include "json-query/utils/SanitizerCompat.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <vector>
#include <QSet>
#include <QDebug>
#include <algorithm>
#include <limits>
#include <utility>

namespace json_query::json_path::detail
{

using internal::acquirePooledArray;
using internal::emptyResult;

// ---------------------------------------------------------------------------
//  Basic evaluation helpers
// ---------------------------------------------------------------------------

int normalizeIndex(int idx, int size) { return idx < 0 ? size + idx : idx; }

// ---------------------------------------------------------------------------
//  evalSlice — RFC 9535 §2.3.5 array slicing
// ---------------------------------------------------------------------------


std::expected<QJsonArray, EvalError> evalSlice(const QJsonArray& array, const Slice& s)
{
    constexpr qsizetype SENTINEL{std::numeric_limits<qsizetype>::max()};
    const auto len{array.size()};

    // RFC 9535: step == 0 selects nothing
    if (s.step == 0)
        return emptyResult();

    auto  pooledArray{acquirePooledArray()};
    auto& out = *pooledArray;

    if (s.step > 0)
    {
        // Forward iteration
        qsizetype start{s.start == SENTINEL ? 0 : s.start};
        qsizetype stop{s.end == SENTINEL ? len : s.end};

        if (start < 0) start += len;
        if (stop < 0)  stop += len;
        if (start < 0) start = 0;
        if (start > len) start = len;
        if (stop < 0) stop = 0;
        if (stop > len) stop = len;

        for (qsizetype i{start}; i < stop;)
        {
            if (i >= 0 && i < len)
                out.append(array[static_cast<int>(i)]);

            // Overflow-safe: stop if adding step would reach or pass stop
            const auto remaining{stop - i};
            if (remaining <= s.step)
                break;
            i += s.step;
        }
    }
    else
    {
        // Backward iteration (step < 0)
        qsizetype start{s.start == SENTINEL ? len - 1 : s.start};
        qsizetype stop{s.end == SENTINEL ? -1 : s.end};

        if (start < 0) start += len;
        if (stop < 0)  stop += len;
        if (start < -1) start = -1;
        if (start >= len) start = len - 1;
        if (stop < -1) stop = -1;
        if (stop >= len) stop = len - 1;

        // Restore sentinel semantics after negative adjustment
        if (s.end == SENTINEL)
            stop = -1;

        for (qsizetype i{start}; i > stop;)
        {
            if (i >= 0 && i < len)
                out.append(array[static_cast<int>(i)]);

            // Underflow-safe: step <= stop - i means no more valid indices
            const auto delta{stop - i};
            if (s.step <= delta)
                break;
            i += s.step;
        }
    }

    return std::move(out);
}

// ---------------------------------------------------------------------------
//  Union processing: evaluate bracket groups like $[0,1,"key"]
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> processUnionTokens(const PathEvalCtx&            ctx,
                                                        const std::vector<qsizetype>& unionTokens,
                                                        const QJsonArray&             working,
                                                        const QJsonValue&             root)
{
    if (unionTokens.empty())
        return std::unexpected(EvalError::TypeMismatchObject);

    // Single-token optimization: skip merge overhead
    if (unionTokens.size() == 1)
    {
        auto result{processSingleUnionToken(ctx, unionTokens[0], working, root)};
        if (result.success)
            return std::move(result.results);
        return std::unexpected(result.error);
    }

    // Multiple tokens: collect results from each, merge at the end
    std::vector<QJsonArray> resultArrays;
    resultArrays.reserve(unionTokens.size());

    auto lastError{EvalError::TypeMismatchObject};
    for (const auto tokenIdx : unionTokens)
    {
        auto result{processSingleUnionToken(ctx, tokenIdx, working, root)};
        if (result.success)
            resultArrays.push_back(std::move(result.results));
        else
            lastError = result.error;
    }

    // RFC 9535: only fail if ALL selectors in the union failed
    if (resultArrays.empty())
        return std::unexpected(lastError);

    // Copy-init: brace-init would trigger QJsonArray's initializer_list constructor (ADR-001)
    auto mergedResults = mergeTokenResults(resultArrays);
    return mergedResults;
}

// ---------------------------------------------------------------------------
//  Union detection and processing helpers
// ---------------------------------------------------------------------------

// Helper: Check if a token is a selector token that can appear in unions
bool isSelectorToken(const Token& token)
{
    return token.kind == Token::Kind::Index || token.kind == Token::Kind::Key || token.kind == Token::Kind::Filter ||
           token.kind == Token::Kind::Wildcard || token.kind == Token::Kind::Slice;
}

UnionDetectionResult detectUnionTokens(const PathEvalCtx& ctx, qsizetype startIndex)
{
    // Collect consecutive selector tokens
    std::vector<qsizetype> unionTokens;
    unionTokens.push_back(startIndex);

    for (auto j{startIndex + 1}; j < ctx.tokens.size() && isSelectorToken(ctx.tokens[j]); ++j)
        unionTokens.push_back(j);

    // A union requires 2+ tokens from the same bracket group
    auto shouldUseUnion{false};
    if (unionTokens.size() > 1)
    {
        const auto firstGroupId{ctx.tokens[unionTokens[0]].bracketGroupId};
        if (firstGroupId > 0)
        {
            shouldUseUnion = true;
            for (qsizetype i{1}; i < static_cast<qsizetype>(unionTokens.size()); ++i)
                if (ctx.tokens[unionTokens[i]].bracketGroupId != firstGroupId)
                {
                    shouldUseUnion = false;
                    break;
                }
        }
    }

    return {unionTokens, shouldUseUnion, startIndex + static_cast<qsizetype>(unionTokens.size())};
}

std::expected<QJsonArray, EvalError> processBranchUniqueSelection(
    const PathEvalCtx& ctx, qsizetype& i, const QJsonArray& working, const QJsonValue& root, bool isLeaf)
{
    auto        keyResult{collectKeysFromTokens(ctx, i)};
    const auto& keys = keyResult.keys;
    i                = keyResult.nextIndex;

    if (keys.empty())
        return std::unexpected(EvalError::KeyNotFound);

    auto  pooledArray{acquirePooledArray()};
    auto& results = *pooledArray;

    for (const auto& workingValue : working)
    {
        if (!workingValue.isObject())
            continue;

        const auto obj{workingValue.toObject()};

        if (isLeaf)
            processObjectForLeafSelection(obj, keys, workingValue, &results);
        else
            processObjectForNonLeafSelection(obj, keys, workingValue, &results);
    }

    // Deduplicate results
    return deduplicateJsonValues(results);
}

bool addsMultiplicity(const Token& tk)
{
    switch (tk.kind)
    {
    case Token::Kind::Wildcard:
    case Token::Kind::Recursive:
    case Token::Kind::KeyList:
    case Token::Kind::Slice:
    case Token::Kind::Filter:
        return true;
    default:
        return false;
    }
}

QJsonValue squash(QJsonArray&& arr, bool multi)
{
    if (multi || arr.size() != 1)
        return arr;
    return arr[0];
}

QJsonValue applyTrailing(json_path::FunctionType fn, const QJsonValue& v)
{
    using enum json_path::FunctionType;

    switch (fn)
    {
    case None:
        return v;

    case Length:
        if (v.isArray())
            return v.toArray().size();
        if (v.isObject())
            return v.toObject().size();
        return 0;

    case Min:
    case Max:
        if (!v.isArray())
            return {QJsonValue::Undefined};
        {
            const auto arr{asArray(v)};
            bool       first{true};
            double     best = 0.0;
            for (const auto& e : arr)
            {
                if (!e.isDouble())
                    continue;
                const auto d{e.toDouble()};
                if (first || (fn == Min ? d < best : d > best))
                    best = d, first = false;
            }
            return first ? QJsonValue{QJsonValue::Undefined} : QJsonValue(best);
        }
    }
    std::unreachable();
}

// Helper: Collect keys from consecutive Key/KeyList tokens
KeyCollectionResult collectKeysFromTokens(const PathEvalCtx& ctx, qsizetype startIndex)
{
    std::vector<QString> keys;
    auto                 i{startIndex};

    while (i < ctx.tokens.size())
    {
        const auto& token = ctx.tokens[i];
        if (token.kind == Token::Kind::Key)
        {
            keys.push_back(token.key);
            ++i;
        }
        else if (token.kind == Token::Kind::KeyList)
        {
            QStringList keyList = token.key.split(u'\n');
            for (const QString& key : keyList)
                keys.push_back(key);
            ++i;
        }
        else
        {
            break;
        }
    }

    return {keys, i};
}

// Helper: Process a single union token and collect its results
TokenProcessingResult
processSingleUnionToken(const PathEvalCtx& ctx, qsizetype tokenIdx, const QJsonArray& working, const QJsonValue& root)
{
    auto  pooledArray{acquirePooledArray()};
    auto& results = *pooledArray;

    auto anySuccess{false};
    auto lastError{EvalError::KeyNotFound};

    for (const auto& workingValue : working)
    {
        auto tokenResult{evaluateToken(ctx, ctx.tokens[tokenIdx], workingValue, tokenIdx)};
        if (!tokenResult)
        {
            lastError = tokenResult.error();
            continue;
        }

        for (const auto& value : *tokenResult)
            results.append(value);
        anySuccess = true;
    }

    if (anySuccess)
        return {.success = true, .results = std::move(results), .error = EvalError::KeyNotFound};
    return {.success = false, .results = QJsonArray{}, .error = lastError};
}

// Helper: Merge results from multiple tokens using simple concatenation
QJsonArray mergeTokenResults(const std::vector<QJsonArray>& resultArrays)
{
    QJsonArray collectedResults;

    // Concatenate results from all union tokens
    for (const auto& results : resultArrays)
        for (const auto& value : results)
            collectedResults.append(value);

    // RFC 9535: Unions preserve order and duplicates; do not deduplicate here
    return collectedResults;
}

// Helper: Check if an object contains all required keys
bool objectContainsAllKeys(const QJsonObject& obj, const std::vector<QString>& keys)
{
    for (const QString& key : keys)
        if (!obj.contains(key))
            return false;
    return true;
}

// Helper: Process object for leaf selection
void processObjectForLeafSelection(const QJsonObject&          obj,
                                   const std::vector<QString>& keys,
                                   const QJsonValue&           v,
                                   QJsonArray*                 results)
{
    if (!objectContainsAllKeys(obj, keys))
        return;

    if (keys.size() > 1)
    {
        // Multi-property leaf union ⇒ return parent only (Jayway)
        results->append(v);
    }
    else
    {
        // Single-property leaf: return only the value, not the parent
        results->append(obj.value(keys[0]));
    }
}

// Helper: Process object for non-leaf selection
void processObjectForNonLeafSelection(const QJsonObject&          obj,
                                      const std::vector<QString>& keys,
                                      const QJsonValue&           v,
                                      QJsonArray*                 results)
{
    for (const QString& key : keys)
        if (obj.contains(key))
            results->append(obj[key]);
}

// Helper: Deduplicate JSON values using hash-based approach
// Primitives are always kept; objects/arrays are deduplicated by content hash.
QJsonArray deduplicateJsonValues(const QJsonArray& input)
{
    QSet<uint> seen;
    seen.reserve(input.size());

    auto  pooledDedup{acquirePooledArray()};
    auto& dedup = *pooledDedup;

    for (const auto& v : input)
    {
        if (!v.isObject() && !v.isArray())
        {
            dedup.append(v);
            continue;
        }

        const auto h{v.isObject() ? qHash(QJsonDocument(v.toObject()).toJson())
                                  : qHash(QJsonDocument(v.toArray()).toJson())};

        if (!seen.contains(h))
        {
            seen.insert(h);
            dedup.append(v);
        }
    }
    return std::move(dedup);
}

} // namespace json_query::json_path::detail
