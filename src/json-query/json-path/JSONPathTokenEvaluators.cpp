// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPathWildcardRecursive.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/internal/FilterSpecializations.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

#include <expected>

namespace json_query::json_path::detail
{

using internal::acquirePooledArray;
using internal::emptyResult;

// --- Key -------------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Key>(const PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v, qsizetype /*tokenPos*/)
{
    if (!v.isObject())
        return emptyResult();

    const auto obj{v.toObject()};
    const auto it{obj.find(tk.key)};
    if (it == obj.end())
        return emptyResult();

    auto  pooledArray{acquirePooledArray()};
    auto& result = *pooledArray;
    result.append(it.value());
    return std::move(result);
}

// --- Index -----------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Index>(const PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v, qsizetype /*tokenPos*/)
{
    if (!v.isArray())
        return emptyResult();

    const auto arr = v.toArray();
    const auto idx{normalizeIndex(tk.index, arr.size())};
    if (idx < 0 || idx >= arr.size())
        return emptyResult();

    auto  pooledArray{acquirePooledArray()};
    auto& out = *pooledArray;
    out.append(arr[idx]);
    return std::move(out);
}

// --- Slice -----------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Slice>(const PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v, qsizetype /*tokenPos*/)
{
    if (!v.isArray())
        return emptyResult();
    return evalSlice(v.toArray(), tk.slice);
}

// --- Wildcard --------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Wildcard>(const PathEvalCtx& /*ctx*/, const Token&, const QJsonValue& v, qsizetype /*tokenPos*/)
{
    // Fast path: direct type checking and processing without monadic overhead
    if (v.isObject())
        return wildcardObject(v.toObject());

    if (v.isArray())
        return wildcardArray(v.toArray());

    // Empty result for primitives (not an error in JSONPath)
    return emptyResult();
}

// --- Recursive -------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Recursive>(const PathEvalCtx& ctx, const Token& /*tk*/, const QJsonValue& v, qsizetype tokenPos)
{
    // Look ahead: if next token is a simple key, build a path hint for "$..key" optimization
    if (tokenPos + 1 < static_cast<qsizetype>(ctx.tokens.size()))
    {
        const auto& nextToken{ctx.tokens[tokenPos + 1]};
        if (nextToken.kind == Token::Kind::Key)
        {
            auto pathHint{QStringLiteral("$..") + nextToken.key};
            return evaluateRecursive(v, QStringView(pathHint));
        }
    }

    return evaluateRecursive(v, 0);
}

// --- Filter ----------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Filter>(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v, qsizetype /*tokenPos*/)
{
    // First try pattern-aware filter optimization
    if (auto result{internal::PatternAwareFilterEvaluator::evaluate(ctx, tk, v)})
        return result;

    // Fall back to embedded filter evaluation
    auto  pooledArray{acquirePooledArray()};
    auto& out = *pooledArray;

    if (!tk.hasEmbeddedFilter())
        return std::move(out);

    const auto needsRootContext{tk.key.contains("value($")};

    auto applyFilter = [&](const QJsonValue& item)
    {
        const auto pass{needsRootContext ? tk.evaluateEmbeddedContextFilter(item, ctx.rootDocument)
                                         : tk.evaluateEmbeddedFilter(item)};
        if (pass)
            out.append(item);
    };

    if (v.isArray())
        for (const auto& item : asArray(v))
            applyFilter(item);
    else if (v.isObject())
    {
        const auto obj{v.toObject()};
        for (auto it{obj.constBegin()}; it != obj.constEnd(); ++it)
            applyFilter(it.value());
    }

    return std::move(out);
}

// --- KeyList ---------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::KeyList>(const PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v, qsizetype /*tokenPos*/)
{
    if (!v.isObject())
        return emptyResult();

    const auto        obj{v.toObject()};
    const QStringList keys = tk.key.split(u'\n');
    auto              pooledArray{acquirePooledArray()};
    auto&             results = *pooledArray;

    for (const QString& key : keys)
        if (obj.contains(key))
            results.append(obj.value(key));

    return std::move(results);
}

} // namespace json_query::json_path::detail
