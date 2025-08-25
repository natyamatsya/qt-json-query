// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathTokenDispatch.hpp"
#include "json-query/json-path/JSONPathWildcardRecursive.hpp"
#include "json-query/json-path/JSONPathEvalHelpers.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/utils/SanitizerCompat.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include "json-query/json-path/internal/ContextAwareContainerCursor.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"
#include "json-query/json-path/internal/PathPatternSpecializations.hpp"

#include "json-query/json-path/JSONPathPointerConversion.hpp"
#include "json-query/json-path/internal/QtHash.hpp"

#include <array>
#include <deque>
#include <expected>
#include <QSet>
#include <QJsonDocument>
#include <QStringList>
#include <QString>
#include <QDebug>
#include <algorithm>
#include "json-query/json-path/internal/IterativeRecursiveDescent.hpp"
#include "json-query/json-path/internal/ArenaAllocator.hpp"

namespace json_query::json_path::detail
{

// Forward declaration for fanOut function used in TokenProcessingStrategy
std::expected<QJsonArray, EvalError>
fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos);

using internal::acquirePooledArray;
using internal::emptyResult;
using internal::IterativeRecursiveDescent;
using json_query::json_path::internal::ContainerCursor;
using json_query::json_path::internal::ResultCollector;

// ---------------------------------------------------------------------------
//  TableGen-Inspired Token Processing Architecture
// ---------------------------------------------------------------------------

// Enum for different token processing strategies
enum class TokenProcessingType
{
    UnionDetection,        // Consecutive tokens that should be unioned
    BranchUniqueSelection, // KeyList/Key after Recursive (branch-unique)
    StandardFanOut,        // Normal token evaluation with fan-out
    RootSelectorOnly       // Special case for root selector ($)
};

// Template for token processing strategy definitions
template <TokenProcessingType Type>
struct TokenProcessingDef
{
    static constexpr bool enabled = false;
};

// Template specializations for each processing strategy
template <>
struct TokenProcessingDef<TokenProcessingType::UnionDetection>
{
    static constexpr bool enabled = true;

    static bool matches(const PathEvalCtx& ctx, qsizetype i, const Token& tk, bool prevRecursive)
    {
        return (tk.kind == Token::Kind::Index || tk.kind == Token::Kind::Key || tk.kind == Token::Kind::Filter ||
                tk.kind == Token::Kind::Wildcard || tk.kind == Token::Kind::Slice);
    }
};

template <>
struct TokenProcessingDef<TokenProcessingType::BranchUniqueSelection>
{
    static constexpr bool enabled = true;

    static bool matches(const PathEvalCtx& ctx, qsizetype i, const Token& tk, bool prevRecursive)
    {
        return prevRecursive && (tk.kind == Token::Kind::KeyList || tk.kind == Token::Kind::Key);
    }
};

template <>
struct TokenProcessingDef<TokenProcessingType::StandardFanOut>
{
    static constexpr bool enabled = true;

    static bool matches(const PathEvalCtx& ctx, qsizetype i, const Token& tk, bool prevRecursive)
    {
        return true; // Default fallback strategy
    }
};

template <>
struct TokenProcessingDef<TokenProcessingType::RootSelectorOnly>
{
    static constexpr bool enabled = true;

    static bool matches(const PathEvalCtx& ctx, qsizetype i, const Token& tk, bool prevRecursive)
    {
        return ctx.tokens.size() == 1;
    }
};

// Template for token processing strategy implementations
template <TokenProcessingType Type>
struct TokenProcessingStrategy
{
    static std::expected<QJsonArray, EvalError> process(const PathEvalCtx& ctx,
                                                        qsizetype&         i,
                                                        const Token&       tk,
                                                        const QJsonArray&  working,
                                                        const QJsonValue&  root,
                                                        bool&              multi,
                                                        bool               prevRecursive) = delete;
};

// Specialization for Union Detection processing
template <>
struct TokenProcessingStrategy<TokenProcessingType::UnionDetection>
{
    static std::expected<QJsonArray, EvalError> process(const PathEvalCtx& ctx,
                                                        qsizetype&         i,
                                                        const Token&       tk,
                                                        const QJsonArray&  working,
                                                        const QJsonValue&  root,
                                                        bool&              multi,
                                                        bool               prevRecursive)
    {

        // Look ahead to see if we have consecutive tokens that should be unioned
        UnionDetectionResult unionDetectionResult = detectUnionTokens(ctx, i);

        if (unionDetectionResult.shouldUseUnion)
        {
            qCDebug(jsonPathLog) << "[union] processing" << unionDetectionResult.unionTokens.size()
                                 << "consecutive selector tokens";
            // Debug: list token kinds in this union group (guarded)
            if (jsonPathLog().isDebugEnabled())
            {
                QString kinds;
                kinds.reserve(64);
                for (qsizetype idx : unionDetectionResult.unionTokens)
                {
                    kinds += QString::number(static_cast<int>(ctx.tokens[idx].kind));
                    kinds += ' ';
                }
                qCDebug(jsonPathLog) << "[union] tokenKinds=" << kinds;
            }

            auto result{processUnionTokens(ctx, unionDetectionResult.unionTokens, working, root)};

            // Skip the tokens we just processed
            i = unionDetectionResult.nextIndex - 1;

            // If any token in the union adds multiplicity, the union as a whole is multiplicity-adding.
            bool unionAddsMultiplicity{false};
            for (qsizetype idx : unionDetectionResult.unionTokens)
            {
                if (addsMultiplicity(ctx.tokens[idx]))
                {
                    unionAddsMultiplicity = true;
                    break;
                }
            }

            bool multiAfter{multi || unionAddsMultiplicity};
            if (result && result->empty())
                return emptyResult(); // RFC 9535: empty result list when no matches

            multi = multiAfter;
            return result;
        }

        // If union detection doesn't apply, fall back to standard processing
        return std::unexpected(EvalError::TypeMismatchObject); // Signal fallback needed
    }
};

// Specialization for Branch-Unique Selection processing
template <>
struct TokenProcessingStrategy<TokenProcessingType::BranchUniqueSelection>
{
    static std::expected<QJsonArray, EvalError> process(const PathEvalCtx& ctx,
                                                        qsizetype&         i,
                                                        const Token&       tk,
                                                        const QJsonArray&  working,
                                                        const QJsonValue&  root,
                                                        bool&              multi,
                                                        bool               prevRecursive)
    {

        bool isLeaf{(i + 1 == ctx.tokens.size())};

        auto result{processBranchUniqueSelection(ctx, i, working, root, isLeaf)};

        if (result && result->empty())
            return emptyResult(); // RFC 9535: empty result list when no matches

        return result;
    }
};

// Specialization for Standard Fan-Out processing
template <>
struct TokenProcessingStrategy<TokenProcessingType::StandardFanOut>
{
    static std::expected<QJsonArray, EvalError> process(const PathEvalCtx& ctx,
                                                        qsizetype&         i,
                                                        const Token&       tk,
                                                        const QJsonArray&  working,
                                                        const QJsonValue&  root,
                                                        bool&              multi,
                                                        bool               prevRecursive)
    {

        bool multiAfter{multi || addsMultiplicity(tk)};

        // C++23 Monadic Chain - Elegant error composition for token evaluation!
        qCDebug(jsonPathLog) << "StandardFanOut: calling fanOut tokenIdx=" << i << "kind=" << static_cast<int>(tk.kind)
                             << "index=" << tk.index;
        auto result = fanOut(ctx, tk, working, i)
                          .and_then(
                              [&](QJsonArray&& result) -> std::expected<QJsonArray, EvalError>
                              {
                                  qCDebug(jsonPathLog) << "StandardFanOut: fanOut result size=" << result.size();
                                  if (result.empty())
                                  {
                                      qCDebug(jsonPathLog) << "StandardFanOut: fanOut result empty -> emptyResult";
                                      return emptyResult(); // RFC 9535: empty result list when no matches
                                  }
                                  qCDebug(jsonPathLog) << "StandardFanOut: fanOut result non-empty";
                                  return std::move(result);
                              })
                          .and_then(
                              [&](QJsonArray&& result) -> std::expected<QJsonArray, EvalError>
                              {
                                  // Deduplicate containers after normal fan-out when preceded by Recursive
                                  if (prevRecursive)
                                  {
                                      // Pre-allocate hash set with reasonable capacity to reduce rehashing
                                      QSet<uint> seen;
                                      seen.reserve(result.size()); // Reserve based on input size

                                      // Use ArrayPool for deduplication array
                                      auto  pooledDedup{acquirePooledArray()};
                                      auto& dedup = *pooledDedup;

                                      for (const auto& v : result)
                                      {
                                          uint h;
                                          if (v.isObject())
                                          {
                                              h = qHash(QJsonDocument(v.toObject()).toJson());
                                          }
                                          else if (v.isArray())
                                          {
                                              h = qHash(QJsonDocument(v.toArray()).toJson());
                                          }
                                          else
                                          {
                                              // For primitive values, add directly without hashing overhead
                                              dedup.append(v);
                                              continue;
                                          }

                                          if (!seen.contains(h))
                                          {
                                              seen.insert(h);
                                              dedup.append(v);
                                          }
                                      }
                                      return std::move(dedup);
                                  }
                                  return std::move(result);
                              })
                          .or_else(
                              [](EvalError error) -> std::expected<QJsonArray, EvalError>
                              {
                                  qCDebug(jsonPathLog)
                                      << "evalStandard: fanOut returned error" << static_cast<int>(error);
                                  return std::unexpected(error);
                              });

        multi = multiAfter;
        return result;
    }
};

// TableGen-inspired recursive dispatch table for token processing strategies
template <TokenProcessingType... Types>
struct TokenProcessingDispatchTable;

template <TokenProcessingType FirstType, TokenProcessingType... RestTypes>
struct TokenProcessingDispatchTable<FirstType, RestTypes...>
{
    static std::expected<QJsonArray, EvalError> dispatch(const PathEvalCtx& ctx,
                                                         qsizetype&         i,
                                                         const Token&       tk,
                                                         const QJsonArray&  working,
                                                         const QJsonValue&  root,
                                                         bool&              multi,
                                                         bool               prevRecursive)
    {
        qCDebug(jsonPathLog) << "Dispatch: tokenIdx=" << i << "kind=" << static_cast<int>(tk.kind)
                             << "index=" << tk.index;
        if constexpr (TokenProcessingDef<FirstType>::enabled)
        {
            qCDebug(jsonPathLog) << "Dispatch: trying strategy (enabled)=" << static_cast<int>(FirstType);
            if (TokenProcessingDef<FirstType>::matches(ctx, i, tk, prevRecursive))
            {
                qCDebug(jsonPathLog) << "Dispatch: strategy matches -> process";
                auto result{
                    TokenProcessingStrategy<FirstType>::process(ctx, i, tk, working, root, multi, prevRecursive)};
                qCDebug(jsonPathLog) << "Dispatch: process result has_value=" << result.has_value();

                // Special handling for union detection fallback
                if constexpr (FirstType == TokenProcessingType::UnionDetection)
                {
                    if (!result)
                    {
                        qCDebug(jsonPathLog) << "Dispatch: UnionDetection failed -> fallback";
                        // Fall back to next strategy in the dispatch table
                        return TokenProcessingDispatchTable<RestTypes...>::dispatch(
                            ctx, i, tk, working, root, multi, prevRecursive);
                    }
                }

                return result;
            }
            else
            {
                qCDebug(jsonPathLog) << "Dispatch: strategy does not match";
            }
        }
        else
        {
            qCDebug(jsonPathLog) << "Dispatch: strategy not enabled=" << static_cast<int>(FirstType);
        }

        qCDebug(jsonPathLog) << "Dispatch: trying next strategy";
        // Try next strategy in the dispatch table
        return TokenProcessingDispatchTable<RestTypes...>::dispatch(ctx, i, tk, working, root, multi, prevRecursive);
    }
};

// Base case: no more strategies to try
template <>
struct TokenProcessingDispatchTable<>
{
    static std::expected<QJsonArray, EvalError> dispatch(const PathEvalCtx& ctx,
                                                         qsizetype&         i,
                                                         const Token&       tk,
                                                         const QJsonArray&  working,
                                                         const QJsonValue&  root,
                                                         bool&              multi,
                                                         bool               prevRecursive)
    {

        return std::unexpected(EvalError::TypeMismatchObject);
    }
};

// Compile-time dispatch table with prioritized strategy ordering
using TokenProcessingDispatcher = TokenProcessingDispatchTable<TokenProcessingType::UnionDetection,
                                                               TokenProcessingType::BranchUniqueSelection,
                                                               TokenProcessingType::StandardFanOut>;

// ---------------------------------------------------------------------------
//  evalStandard – Refactored with TableGen-inspired architecture
// ---------------------------------------------------------------------------
std::expected<QJsonValue, EvalError> evalStandard(const PathEvalCtx& ctx, const QJsonValue& root)
{
    if (ctx.tokens.empty())
        return QJsonValue{QJsonValue::Undefined};

    // Phase 3: Path Pattern Specialization - Fast path for common patterns
    // Temporarily disabled during container migration
    // if (auto patternResult = internal::PatternAwarePathEvaluator::evaluate(ctx, ctx.tokens, root)) {
    //     return *patternResult;
    // }

    // If pattern specialization didn't handle it, fall back to generic evaluation

    // Use ArrayPool for better memory management of working array
    auto  pooledWorkingArray{acquirePooledArray()};
    auto& workingArray = *pooledWorkingArray;

    // Start with the root value as a single node in the working set.
    // RFC 9535 semantics: the root selector "$" selects the document itself; we must NOT
    // pre-flatten root arrays into their elements before applying selectors like [index]/[slice].
    qCDebug(jsonPathLog) << "evalStandard: root type=" << root.type() << "isArray=" << root.isArray();
    if (root.isArray())
        qCDebug(jsonPathLog) << "evalStandard: root is array; keeping as single node";
    workingArray.append(root);

    // SANITIZER WORKAROUND: Avoid QJsonArray copy constructor corruption
    // This is the same issue we fixed in JSON Pointer and array indexing - sanitizer instrumentation
    // corrupts QJsonArray copy constructor, causing wrong size/content in the copied array.
    // SOLUTION: Work directly with the original array to avoid the problematic copy constructor.
    std::expected<QJsonArray, EvalError> working = std::move(workingArray);
    auto                                 multi{false};

    using json_query::json_path::internal::qt_hash;

    for (qsizetype i = 1; i < ctx.tokens.size() && working; ++i)
    {
        const auto& tk = ctx.tokens[i];
        qCDebug(jsonPathLog) << "[stage] token" << i << ": kind=" << static_cast<int>(tk.kind)
                             << "working size=" << working->size();

        auto prevRecursive{(i > 0 && ctx.tokens[i - 1].kind == Token::Kind::Recursive)};

        qCDebug(jsonPathLog) << "[evalStandard] Token" << i << "kind=" << static_cast<int>(tk.kind)
                             << "prevRecursive=" << prevRecursive;

        // TableGen-inspired dispatch: Use compile-time strategy selection
        working = TokenProcessingDispatcher::dispatch(ctx, i, tk, *working, root, multi, prevRecursive);

        if (!working)
            return std::unexpected(working.error());

        if (working->empty())
            return emptyResult(); // RFC 9535: empty result list when no matches
    }

    // Special case: for root selector ($), we should return the root document itself
    // not wrapped in an array, because the root selector should return the complete document
    // as a single result
    auto isRootSelectorOnly{(ctx.tokens.size() == 1)};
    if (isRootSelectorOnly)
    {
        // Return the first (and only) element from the working array, which is the root document
        if (!working->empty())
            return working->first();
        return QJsonValue{QJsonValue::Undefined};
    }

    auto collapsed = squash(*std::move(working), multi);
    if (collapsed.isUndefined())
        return emptyResult(); // RFC 9535: no matches

    return applyTrailing(ctx.trailingFn, collapsed);
}

// ---------------------------------------------------------------------------
//  evaluateDefinite - Evaluate a JSONPath with no wildcards or filters
// ---------------------------------------------------------------------------
std::expected<QJsonValue, EvalError> evaluateDefinite(const std::vector<Token>& tokens,
                                                      const QJsonValue&         root) noexcept
{
    using enum Token::Kind;

    auto cur{root};
    // Skip leading root token ('$' or '@') if present
    auto startIdx{0};
    if (!tokens.empty() && tokens.front().kind == Token::Kind::Key)
    {
        const auto& k = tokens.front().key;
        if (k == u"$" || k == u"@")
            startIdx = 1;
    }

    for (int i = startIdx; i < tokens.size(); ++i)
    {
        const auto& tk = tokens[i];
        switch (tk.kind)
        {
        case Key:
        {
            if (!cur.isObject())
                return std::unexpected(EvalError::TypeMismatchObject);
            const auto obj{cur.toObject()};
            auto       it{obj.constFind(tk.key)};
            if (it == obj.constEnd())
                return std::unexpected(EvalError::KeyNotFound);
            cur = *it;
            break;
        }
        case Index:
        {
            if (!cur.isArray())
                return std::unexpected(EvalError::TypeMismatchArray);
            const auto arr{cur.toArray()};
            auto       idx = normalizeIndex(tk.index, arr.size());
            if (idx < 0 || idx >= arr.size())
                return std::unexpected(EvalError::IndexOutOfRange);
            cur = arr[idx];
            break;
        }
        default:
            // This function only handles definite paths (no wildcards/filters)
            return std::unexpected(EvalError::TypeMismatchObject);
        }
    }
    return cur;
}

// ---------------------------------------------------------------------------
//  Convenience entry points (pure)
// ---------------------------------------------------------------------------

// Note: evaluate(), evaluateAll(), and fanOut() implementations moved to
// JSONPathEvaluate.inl for inlining optimization. The inline implementations
// are included via the header file to enable compiler inlining while keeping
// headers clean.

} // namespace json_query::json_path::detail
