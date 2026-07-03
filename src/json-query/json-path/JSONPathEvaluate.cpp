// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include "json-query/json-path/JSONPathEvalHelpers.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"

#include <expected>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path::detail
{

using internal::acquirePooledArray;
using internal::emptyResult;

// ---------------------------------------------------------------------------
//  Token processing: apply one selector to the working node set
// ---------------------------------------------------------------------------

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
        auto       result{processBranchUniqueSelection(ctx, i, working, root, isLeaf)};
        if (result && result->empty())
            return emptyResult();
        return result;
    }

    // 3. Standard fan-out: apply the selector to each node in the working set
    multi = multi || addsMultiplicity(tk);

    auto result{fanOut(ctx, tk, working, i)};
    if (result->empty())
        return emptyResult();

    // No dedup after recursive descent: the descendant list holds each node
    // once, and fan-out emits each node once as its parent's child. Equal
    // values at distinct locations are distinct nodes per RFC 9535 and must
    // all be kept.
    return std::move(*result);
}

// ---------------------------------------------------------------------------
//  isDefinitePath — true when every selector after $ is Key or Index
// ---------------------------------------------------------------------------
static bool isDefinitePath(const std::vector<Token>& tokens)
{
    for (qsizetype i{1}; i < static_cast<qsizetype>(tokens.size()); ++i)
    {
        const auto& tk{tokens[i]};
        if (tk.kind != Token::Kind::Key && tk.kind != Token::Kind::Index)
            return false;
        // Multi-token bracket groups are unions (e.g., $[0,2]) — not definite.
        // Single-token bracket groups (e.g., $[5]) are still definite.
        if (tk.bracketGroupId > 0 && i + 1 < static_cast<qsizetype>(tokens.size()) &&
            tokens[i + 1].bracketGroupId == tk.bracketGroupId)
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
//  evaluateDefinite — fast path for paths with only Key/Index selectors
// ---------------------------------------------------------------------------
std::expected<NodeList, DetailedEvalError> evaluateDefinite(const std::vector<Token>& tokens,
                                                            const QJsonValue&         root) noexcept
{
    using enum Token::Kind;

    auto cur{root};

    for (qsizetype i{1}; i < static_cast<qsizetype>(tokens.size()); ++i)
    {
        const auto& tk{tokens[i]};
        switch (tk.kind)
        {
        case Key:
        {
            if (!cur.isObject())
                return NodeList{};
            const auto obj{cur.toObject()};
            const auto it{obj.constFind(tk.key)};
            if (it == obj.constEnd())
                return NodeList{};
            cur = *it;
            break;
        }
        case Index:
        {
            if (!cur.isArray())
                return NodeList{};
            const auto arr{asArray(cur)};
            const auto normalIdx{normalizeIndex(tk.index, arr.size())};
            if (normalIdx < 0 || normalIdx >= arr.size())
                return NodeList{};
            cur = arr[normalIdx];
            break;
        }
        default:
            return NodeList{};
        }
    }
    return NodeList{QJsonArray{cur}, false};
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

    // Fast path: definite paths (Key/Index only) skip the full pipeline
    if (isDefinitePath(ctx.tokens))
        return evaluateDefinite(ctx.tokens, root);

    // Initialize working set with the root as the sole node (RFC 9535 §2.2)
    auto  pooledWorkingArray{acquirePooledArray()};
    auto& workingArray = *pooledWorkingArray;
    workingArray.append(root);

    std::expected<QJsonArray, EvalError> working = std::move(workingArray);
    auto                                 multi{false};

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
//  Convenience entry points (pure)
// ---------------------------------------------------------------------------

// Note: evaluate(), evaluateSingle(), and fanOut() implementations moved to
// JSONPathEvaluate.inl for inlining optimization. The inline implementations
// are included via the header file to enable compiler inlining while keeping
// headers clean.

} // namespace json_query::json_path::detail
