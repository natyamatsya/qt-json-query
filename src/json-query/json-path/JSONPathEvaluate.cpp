// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/utils/BraceSafe.hpp"
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
//  Token processing: apply one selector to the working node set
// ---------------------------------------------------------------------------


static std::expected<QJsonArray, EvalError> deduplicateAfterRecursive(QJsonArray&& result)
{
    QSet<uint> seen;
    seen.reserve(result.size());

    auto  pooledDedup{acquirePooledArray()};
    auto& dedup = *pooledDedup;

    for (const auto& v : result)
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

static std::expected<QJsonArray, EvalError> processToken(const PathEvalCtx& ctx,
                                                          qsizetype&         i,
                                                          const QJsonArray&  working,
                                                          const QJsonValue&  root,
                                                          bool&              multi,
                                                          bool               prevRecursive)
{
    const auto& tk{ctx.tokens[i]};

    // 1. Union: consecutive selectors in the same bracket group (e.g., $[0,1,"key"])
    if (isSelectorToken(tk))
    {
        auto detection{detectUnionTokens(ctx, i)};
        if (detection.shouldUseUnion)
        {
            qCDebug(jsonPathLog) << "[union] processing" << detection.unionTokens.size() << "selectors";

            auto result{processUnionTokens(ctx, detection.unionTokens, working, root)};
            i = detection.nextIndex - 1;

            for (const auto idx : detection.unionTokens)
                if (addsMultiplicity(ctx.tokens[idx]))
                {
                    multi = true;
                    break;
                }

            if (result && result->empty())
                return emptyResult();
            return result;
        }
    }

    // 2. Branch-unique selection after recursive descent ($..*["key"])
    if (prevRecursive && (tk.kind == Token::Kind::KeyList || tk.kind == Token::Kind::Key))
    {
        const auto isLeaf{i + 1 == ctx.tokens.size()};
        auto result{processBranchUniqueSelection(ctx, i, working, root, isLeaf)};
        if (result && result->empty())
            return emptyResult();
        return result;
    }

    // 3. Standard fan-out: apply the selector to each node in the working set
    multi = multi || addsMultiplicity(tk);

    auto result{fanOut(ctx, tk, working, i)};
    if (!result)
        return std::unexpected(result.error());
    if (result->empty())
        return emptyResult();

    // Deduplicate containers after recursive descent
    if (prevRecursive)
        return deduplicateAfterRecursive(std::move(*result));

    return std::move(*result);
}

// ---------------------------------------------------------------------------
//  evaluateTokenStream — walk tokens left-to-right over a working node set
// ---------------------------------------------------------------------------
std::expected<NodeList, DetailedEvalError> evaluateTokenStream(const PathEvalCtx& ctx, const QJsonValue& root)
{
    if (ctx.tokens.empty())
        return NodeList{};

    // Root-only selector ($): the document itself is the sole node
    if (ctx.tokens.size() == 1)
        return NodeList{QJsonArray{root}, false};

    // Initialize working set with the root as the sole node (RFC 9535 §2.2)
    auto  pooledWorkingArray{acquirePooledArray()};
    auto& workingArray = *pooledWorkingArray;
    workingArray.append(root);

    std::expected<QJsonArray, EvalError> working = std::move(workingArray);
    auto multi{false};

    // Apply each token (selector) to the working node set
    for (qsizetype i{1}; i < ctx.tokens.size() && working; ++i)
    {
        const auto prevRecursive{i > 0 && ctx.tokens[i - 1].kind == Token::Kind::Recursive};

        working = processToken(ctx, i, *working, root, multi, prevRecursive);

        if (!working)
            return std::unexpected(DetailedEvalError{working.error(), static_cast<std::uint16_t>(i)});

        if (working->empty())
            return NodeList{};
    }

    return NodeList{*std::move(working), multi};
}

// ---------------------------------------------------------------------------
//  evaluateDefinite - Evaluate a JSONPath with no wildcards or filters
// ---------------------------------------------------------------------------
std::expected<QJsonValue, DetailedEvalError> evaluateDefinite(const std::vector<Token>& tokens,
                                                              const QJsonValue&         root) noexcept
{
    using enum Token::Kind;

    auto cur{root};
    // Skip leading root token ('$' or '@') if present
    auto startIdx{0};
    if (!tokens.empty() && tokens.front().kind == Token::Kind::Key)
    {
        const auto& k{tokens.front().key};
        if (k == u"$" || k == u"@")
            startIdx = 1;
    }

    for (int i{startIdx}; i < tokens.size(); ++i)
    {
        const auto  idx16{static_cast<std::uint16_t>(i)};
        const auto& tk{tokens[i]};
        switch (tk.kind)
        {
        case Key:
        {
            if (!cur.isObject())
                return std::unexpected(DetailedEvalError{EvalError::TypeMismatchObject, idx16});
            const auto obj{cur.toObject()};
            auto       it{obj.constFind(tk.key)};
            if (it == obj.constEnd())
                return std::unexpected(DetailedEvalError{EvalError::KeyNotFound, idx16});
            cur = *it;
            break;
        }
        case Index:
        {
            if (!cur.isArray())
                return std::unexpected(DetailedEvalError{EvalError::TypeMismatchArray, idx16});
            const auto arr{asArray(cur)};
            auto       normalIdx{normalizeIndex(tk.index, arr.size())};
            if (normalIdx < 0 || normalIdx >= arr.size())
                return std::unexpected(DetailedEvalError{EvalError::IndexOutOfRange, idx16});
            cur = arr[normalIdx];
            break;
        }
        default:
            // This function only handles definite paths (no wildcards/filters)
            return std::unexpected(DetailedEvalError{EvalError::TypeMismatchObject, idx16});
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
