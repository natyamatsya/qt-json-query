// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"                     // normalizeIndex, evalSlice
#include "json-query/json-path/internal/ContainerCursor.hpp"             // ContainerCursor for optimized iteration
#include "json-query/json-path/internal/ContextAwareContainerCursor.hpp" // ContextAwareContainerCursor for context-aware iteration
#include "json-query/json-path/internal/ArrayPool.hpp"                   // acquirePooledArray, emptyResult
#include "json-query/json-path/internal/FilterSpecializations.hpp"
#include <expected>

namespace json_query::json_path::detail
{

using internal::acquirePooledArray;
using internal::emptyResult;
using json_query::json_path::internal::ContainerCursor;

// --- Key -------------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Key>(const PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v)
{
    // Fast path: direct object key lookup without unnecessary allocations
    if (!v.isObject())
        return emptyResult(); // Empty result for non-objects

    const auto obj{v.toObject()};
    const auto it{obj.find(tk.key)};
    if (it == obj.end())
        return emptyResult(); // Key not found

    // Use ArrayPool for result to optimize memory allocation
    auto  pooledArray{acquirePooledArray()};
    auto& result = *pooledArray;
    result.append(it.value());

    // SANITIZER WORKAROUND: Avoid QJsonArray copy constructor corruption
    // Same issue as in main evaluation pipeline - sanitizer corrupts QJsonArray copy constructor
    // SOLUTION: Return the pooled array directly via std::move to avoid copy constructor
    return std::move(result);
}

// --- Index -----------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Index>(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    // RFC 9535 compliance: "Nothing is selected from a value that is not an array"
    if (!v.isArray())
        return emptyResult(); // Empty result for non-arrays (not an error per RFC 9535)

    // SANITIZER WORKAROUND: Work directly with original array to avoid Qt copy constructor corruption
    // This is the same issue we fixed in JSON Pointer evaluation - sanitizer instrumentation
    // corrupts QJsonArray copy constructor, causing wrong size/content in the copied array.
    const QJsonArray originalArray = v.toArray();
    const auto       idx           = normalizeIndex(tk.index, originalArray.size());

    // RFC 9535 compliance: "Nothing is selected, and it is not an error, if the index lies outside the range of the
    // array"
    if (idx < 0 || idx >= originalArray.size())
        return emptyResult(); // Empty result for out-of-range (not an error per RFC 9535)

    QJsonArray out;
    out.append(originalArray[idx]);
    return out;
}

// --- Slice -----------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Slice>(const PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v)
{
    // Monadic approach: extract array and apply slice if present
    auto asArray = [&v]() -> std::optional<QJsonArray>
    { return v.isArray() ? std::make_optional(v.toArray()) : std::nullopt; };

    return asArray()
        .and_then([&tk](const QJsonArray& arr) -> std::optional<std::expected<QJsonArray, EvalError>>
                  { return std::make_optional(evalSlice(arr, tk.slice)); })
        .value_or(std::expected<QJsonArray, EvalError>{
            emptyResult()}); // Empty result for non-arrays (not an error in JSONPath)
}

// --- Wildcard --------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Wildcard>(const PathEvalCtx& /*ctx*/, const Token&, const QJsonValue& v)
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
eval<Token::Kind::Recursive>(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    // Phase 2 optimization: Build path hint for pattern detection
    auto pathHint = QStringLiteral("$..");

    // Look ahead in token stream to detect common patterns like "$..title"
    auto currentPos{-1};
    for (qsizetype i = 0; i < ctx.tokens.size(); ++i)
    {
        if (&ctx.tokens[i] == &tk)
        {
            currentPos = i;
            break;
        }
    }

    // If we found the current token and there's a next token
    if (currentPos >= 0 && currentPos + 1 < ctx.tokens.size())
    {
        const auto& nextToken = ctx.tokens[currentPos + 1];

        // Check if next token is a simple key (common pattern)
        if (nextToken.kind == Token::Kind::Key)
        {
            pathHint += nextToken.key;

            // Use optimized path with hint
            return evaluateRecursive(v, QStringView(pathHint));
        }
    }

    // Fallback to standard implementation for complex patterns
    return evaluateRecursive(v, 0);
}

// --- Filter ----------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::Filter>(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    qDebug() << "DEBUG: Filter token evaluation called with token key:" << tk.key << "value type:" << v.type();
    
    // First try pattern-aware filter optimization
    if (auto result = internal::PatternAwareFilterEvaluator::evaluate(ctx, tk, v)) {
        qDebug() << "DEBUG: PatternAwareFilterEvaluator handled filter, returning early";
        return result;
    }

    qDebug() << "DEBUG: PatternAwareFilterEvaluator did not handle filter, falling back to embedded filter evaluation";
    
    // Fall back to embedded filter evaluation for complex patterns
    // Use ArrayPool for result to optimize memory allocation
    auto  pooledArray{acquirePooledArray()};
    auto& out = *pooledArray;

    // Check for embedded filters (zero-overhead)
    if (tk.hasEmbeddedFilter())
    {
        qDebug() << "DEBUG: Token has embedded filter, processing array/object";
        // Pre-compute context requirement check to avoid repeated string operations
        const auto needsRootContext{tk.key.contains("value($")};

        if (v.isArray())
        {
            const auto arr{v.toArray()};
            qDebug() << "DEBUG: Processing array with" << arr.size() << "elements";

            for (const auto& item : arr)
            {
                qDebug() << "DEBUG: Processing array item:" << item;
                const bool pass = needsRootContext ? tk.evaluateEmbeddedContextFilter(item, ctx.rootDocument)
                                                   : tk.evaluateEmbeddedFilter(item);
                qDebug() << "DEBUG: Array item filter result:" << pass;
                if (pass)
                    out.append(item);
            }
            qDebug() << "DEBUG: Array processing complete, output has" << out.size() << "items";
        }
        else if (v.isObject())
        {
            const auto obj{v.toObject()};
            qDebug() << "DEBUG: Processing single object:" << QJsonValue(obj);

            // CRITICAL FIX: Evaluate filter on the whole object, not property values
            // For JSONPath filters like $[?@.a || @.b && @.c], the filter should be evaluated
            // once per object with the complete object passed to the embedded filter
            const QJsonValue objValue{obj};
            qDebug() << "DEBUG: Evaluating filter on complete object:" << objValue;
            const bool pass = needsRootContext ? tk.evaluateEmbeddedContextFilter(objValue, ctx.rootDocument)
                                               : tk.evaluateEmbeddedFilter(objValue);
            qDebug() << "DEBUG: Object filter result:" << pass;
            if (pass)
                out.append(objValue);
            
            qDebug() << "DEBUG: Object processing complete, output has" << out.size() << "items";
        }

        // SANITIZER WORKAROUND: Avoid QJsonArray copy constructor corruption
        // Same issue as in main evaluation pipeline - sanitizer corrupts QJsonArray copy constructor
        // SOLUTION: Return the pooled array directly via std::move to avoid copy constructor
        return std::move(out);
    }

    // SANITIZER WORKAROUND: Avoid QJsonArray copy constructor corruption
    // Same issue as in main evaluation pipeline - sanitizer corrupts QJsonArray copy constructor
    // SOLUTION: Return the pooled array directly via std::move to avoid copy constructor
    return std::move(out);
}

// --- KeyList ---------------------------------------------------------------
template <>
std::expected<QJsonArray, EvalError>
eval<Token::Kind::KeyList>(const PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v)
{
    // Direct type check and processing - avoid monadic overhead
    if (!v.isObject())
        return emptyResult(); // Empty result for non-objects

    const auto        obj{v.toObject()};
    const QStringList keys = tk.key.split(u'\n');
    auto              pooledArray{acquirePooledArray()};
    auto&             results = *pooledArray;

    for (const QString& key : keys)
        if (obj.contains(key))
            results.append(obj.value(key));

    // SANITIZER WORKAROUND: Avoid QJsonArray copy constructor corruption
    // Same issue as in main evaluation pipeline - sanitizer corrupts QJsonArray copy constructor
    // SOLUTION: Return the pooled array directly via std::move to avoid copy constructor
    return std::move(results);
}

} // namespace json_query::json_path::detail
