// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "json-query/Common.h"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include <expected>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <utility>

namespace json_query::json_path::internal
{

/**
 * @brief Filter pattern classification for compile-time optimization
 */
enum class FilterPattern
{
    SimpleExistence, // [?(@.key)]
    Generic          // Complex patterns requiring generic evaluation
};

/**
 * @brief Filter pattern detector for compile-time classification
 */
class FilterPatternDetector
{
  public:
    /**
     * @brief Detect the pattern type of a filter expression
     *
     * @param filterKey The filter expression string
     * @return The detected filter pattern type
     */
    QT_QUERY_JSON_ALWAYS_INLINE
    static FilterPattern detectPattern(const QString& filterKey) noexcept
    {
        // Fast pattern detection using string characteristics
        if (filterKey.isEmpty())
            return FilterPattern::Generic;

        // Be very conservative - only optimize the most basic patterns
        // Complex expressions should fall back to generic evaluation

        // Skip any expressions with logical operators (&&, ||, !)
        if (filterKey.contains("&&") || filterKey.contains("||") || filterKey.contains(" and ") ||
            filterKey.contains(" or ") || filterKey.contains("!"))
        {
            return FilterPattern::Generic;
        }

        // Skip expressions with parentheses (grouping)
        if (filterKey.contains('(') || filterKey.contains(')'))
            return FilterPattern::Generic;

        // Skip expressions with function calls or complex operations
        if (filterKey.contains("length()") || filterKey.contains("value(") || filterKey.contains("size()") ||
            filterKey.contains("count("))
        {
            return FilterPattern::Generic;
        }

        // Skip context references for now (too complex)
        if (filterKey.contains("$."))
            return FilterPattern::Generic;

        // Only handle the most basic patterns
        // Simple existence: [?(@.key)] - just @.key with no operators
        if (filterKey.startsWith("@.") && !filterKey.contains('=') && !filterKey.contains('<') &&
            !filterKey.contains('>') && !filterKey.contains(' '))
        {
            // However, exclude wildcard patterns like @.* for now as they need special handling
            if (filterKey.contains('*'))
                return FilterPattern::Generic;
            return FilterPattern::SimpleExistence;
        }

        // For now, fall back to generic for all other cases
        return FilterPattern::Generic;
    }
};

/**
 * @brief Specialized filter evaluators for common patterns
 */
template <FilterPattern Pattern>
class FilterPatternEvaluator
{
  public:
    QT_QUERY_JSON_ALWAYS_INLINE
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
    {
        // Default implementation falls back to generic evaluation
        return std::unexpected(EvalError::TypeMismatchObject);
    }
};

/**
 * @brief Simple existence filter optimization: [?(@.key)]
 */
template <>
class FilterPatternEvaluator<FilterPattern::SimpleExistence>
{
  public:
    QT_QUERY_JSON_ALWAYS_INLINE
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v) noexcept
    {
        auto        pooledArray{acquirePooledArray()};
        QJsonArray& out = *pooledArray;

        // Extract key from filter expression (remove @. prefix)
        auto key{tk.key};
        if (key.startsWith("@."))
            key = key.mid(2);

        if (v.isArray())
        {
            const auto arr{asArray(v)};
            for (const auto& item : arr)
            {
                if (item.isObject())
                {
                    const auto obj{item.toObject()};
                    if (obj.contains(key))
                        out.append(item);
                }
            }
        }
        else if (v.isObject())
        {
            const auto obj{v.toObject()};
            for (auto it{obj.begin()}; it != obj.end(); ++it)
            {
                const auto& val{it.value()};
                if (val.isObject())
                {
                    const auto valObj{val.toObject()};
                    if (valObj.contains(key))
                        out.append(val);
                }
            }
        }

        return std::move(out);
    }
};

/**
 * @brief Pattern-aware filter evaluator dispatcher
 */
class PatternAwareFilterEvaluator
{
  public:
    QT_QUERY_JSON_ALWAYS_INLINE
    static std::expected<QJsonArray, EvalError>
    evaluate(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
    {
        if (FilterPatternDetector::detectPattern(tk.key) == FilterPattern::SimpleExistence)
            return FilterPatternEvaluator<FilterPattern::SimpleExistence>::eval(ctx, tk, v);

        // Fall back to generic embedded filter evaluation
        return std::unexpected(EvalError::TypeMismatchObject);
    }
};

} // namespace json_query::json_path::internal
