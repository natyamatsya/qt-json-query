// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/Common.h"
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include <expected>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QRegularExpression>

namespace json_query::json_path::internal
{

/**
 * @brief Filter pattern classification for compile-time optimization
 */
enum class FilterPattern
{
    SimpleExistence,   // [?(@.key)]
    SimpleComparison,  // [?(@.key == "value")]
    NumericComparison, // [?(@.age > 18)]
    LengthComparison,  // [?(@.array.length() > 0)]
    ContextReference,  // [?(@.key == $.other)]
    Generic            // Complex patterns requiring generic evaluation
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
            const auto arr{v.toArray()};
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
            for (auto it = obj.begin(); it != obj.end(); ++it)
            {
                const QJsonValue& val = it.value();
                if (val.isObject())
                {
                    const auto valObj{val.toObject()};
                    if (valObj.contains(key))
                        out.append(val);
                }
            }
        }

        return QJsonArray(out);
    }
};

/**
 * @brief Simple comparison filter optimization: [?(@.key == "value")]
 */
template <>
class FilterPatternEvaluator<FilterPattern::SimpleComparison>
{
  public:
    QT_QUERY_JSON_ALWAYS_INLINE
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v) noexcept
    {

        auto        pooledArray{acquirePooledArray()};
        QJsonArray& out = *pooledArray;

        // Parse key and value from expression like "@.key == 'value'"
        const auto expr{tk.key};
        const auto eqPos{expr.indexOf("==")};
        if (eqPos == -1)
            return QJsonArray(out); // Invalid expression

        auto key{expr.left(eqPos).trimmed()};
        auto value{expr.mid(eqPos + 2).trimmed()};

        // Remove @. prefix from key
        if (key.startsWith("@."))
            key = key.mid(2);

        // Remove quotes from value
        if ((value.startsWith('"') && value.endsWith('"')) || (value.startsWith('\'') && value.endsWith('\'')))
            value = value.mid(1, value.length() - 2);

        if (v.isArray())
        {
            const auto arr{v.toArray()};
            for (const auto& item : arr)
            {
                if (item.isObject())
                {
                    const auto obj{item.toObject()};
                    const auto it{obj.find(key)};
                    if (it != obj.end() && it.value().toString() == value)
                        out.append(item);
                }
            }
        }
        else if (v.isObject())
        {
            const auto obj{v.toObject()};
            for (auto it = obj.begin(); it != obj.end(); ++it)
            {
                const QJsonValue& val = it.value();
                if (val.isObject())
                {
                    const auto valObj{val.toObject()};
                    const auto keyIt{valObj.find(key)};
                    if (keyIt != valObj.end() && keyIt.value().toString() == value)
                        out.append(val);
                }
            }
        }

        return QJsonArray(out);
    }
};

/**
 * @brief Numeric comparison filter optimization: [?(@.age > 18)]
 */
template <>
class FilterPatternEvaluator<FilterPattern::NumericComparison>
{
  public:
    QT_QUERY_JSON_ALWAYS_INLINE
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v) noexcept
    {

        auto        pooledArray{acquirePooledArray()};
        QJsonArray& out = *pooledArray;

        // Parse numeric comparison expression
        const auto expr{tk.key};
        QString    key;
        QString    op;
        auto       compareValue{0.0};

        // Simple parsing for common operators
        if (expr.contains(" > "))
        {
            const QStringList parts = expr.split(" > ");
            if (parts.size() == 2)
            {
                key          = parts[0].trimmed();
                op           = ">";
                compareValue = parts[1].trimmed().toDouble();
            }
        }
        else if (expr.contains(" < "))
        {
            const QStringList parts = expr.split(" < ");
            if (parts.size() == 2)
            {
                key          = parts[0].trimmed();
                op           = "<";
                compareValue = parts[1].trimmed().toDouble();
            }
        }
        else if (expr.contains(" >= "))
        {
            const QStringList parts = expr.split(" >= ");
            if (parts.size() == 2)
            {
                key          = parts[0].trimmed();
                op           = ">=";
                compareValue = parts[1].trimmed().toDouble();
            }
        }
        else if (expr.contains(" <= "))
        {
            const QStringList parts = expr.split(" <= ");
            if (parts.size() == 2)
            {
                key          = parts[0].trimmed();
                op           = "<=";
                compareValue = parts[1].trimmed().toDouble();
            }
        }

        // Remove @. prefix from key
        if (key.startsWith("@."))
            key = key.mid(2);

        if (key.isEmpty() || op.isEmpty())
            return QJsonArray(out); // Invalid expression

        if (v.isArray())
        {
            const auto arr{v.toArray()};
            for (const auto& item : arr)
            {
                if (item.isObject())
                {
                    const auto obj{item.toObject()};
                    const auto it{obj.find(key)};
                    if (it != obj.end())
                    {
                        const auto itemValue{it.value().toDouble()};
                        auto       matches{false};

                        if (op == ">")
                            matches = itemValue > compareValue;
                        else if (op == "<")
                            matches = itemValue < compareValue;
                        else if (op == ">=")
                            matches = itemValue >= compareValue;
                        else if (op == "<=")
                            matches = itemValue <= compareValue;

                        if (matches)
                            out.append(item);
                    }
                }
            }
        }

        return QJsonArray(out);
    }
};

/**
 * @brief Length comparison filter optimization: [?(@.array.length() > 0)]
 */
template <>
class FilterPatternEvaluator<FilterPattern::LengthComparison>
{
  public:
    QT_QUERY_JSON_ALWAYS_INLINE
    static std::expected<QJsonArray, EvalError>
    eval(const detail::PathEvalCtx& /*ctx*/, const Token& tk, const QJsonValue& v) noexcept
    {

        auto        pooledArray{acquirePooledArray()};
        QJsonArray& out = *pooledArray;

        // Parse length comparison expression
        const auto expr{tk.key};
        const auto lengthPos{expr.indexOf(".length()")};
        if (lengthPos == -1)
            return QJsonArray(out); // Invalid expression

        auto    key{expr.left(lengthPos).trimmed()};
        QString remainder{expr.mid(lengthPos + 9).trimmed()}; // Skip ".length()"

        // Remove @. prefix from key
        if (key.startsWith("@."))
            key = key.mid(2);

        // Parse comparison operator and value
        QString op;
        auto    compareValue{0};

        if (remainder.startsWith(" > "))
        {
            op           = ">";
            compareValue = remainder.mid(3).trimmed().toInt();
        }
        else if (remainder.startsWith(" < "))
        {
            op           = "<";
            compareValue = remainder.mid(3).trimmed().toInt();
        }
        else if (remainder.startsWith(" >= "))
        {
            op           = ">=";
            compareValue = remainder.mid(4).trimmed().toInt();
        }
        else if (remainder.startsWith(" <= "))
        {
            op           = "<=";
            compareValue = remainder.mid(4).trimmed().toInt();
        }
        else if (remainder.startsWith(" == "))
        {
            op           = "==";
            compareValue = remainder.mid(4).trimmed().toInt();
        }

        if (op.isEmpty())
            return QJsonArray(out); // Invalid expression

        if (v.isArray())
        {
            const auto arr{v.toArray()};
            for (const auto& item : arr)
            {
                if (item.isObject())
                {
                    const auto obj{item.toObject()};
                    const auto it{obj.find(key)};
                    if (it != obj.end() && it.value().isArray())
                    {
                        const auto arrayLength{it.value().toArray().size()};
                        auto       matches{false};

                        if (op == ">")
                            matches = arrayLength > compareValue;
                        else if (op == "<")
                            matches = arrayLength < compareValue;
                        else if (op == ">=")
                            matches = arrayLength >= compareValue;
                        else if (op == "<=")
                            matches = arrayLength <= compareValue;
                        else if (op == "==")
                            matches = arrayLength == compareValue;

                        if (matches)
                            out.append(item);
                    }
                }
            }
        }

        return QJsonArray(out);
    }
};

/**
 * @brief Pattern-aware filter evaluator dispatcher
 */
class PatternAwareFilterEvaluator
{
  public:
    /**
     * @brief Evaluate a filter using pattern-specific optimizations
     *
     * @param ctx Path evaluation context
     * @param tk Filter token
     * @param v JSON value to filter
     * @return Expected result or error
     */
    QT_QUERY_JSON_ALWAYS_INLINE
    static std::expected<QJsonArray, EvalError>
    evaluate(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
    {

        // Detect the filter pattern type
        const FilterPattern pattern = FilterPatternDetector::detectPattern(tk.key);

        // Dispatch to specialized evaluator
        switch (pattern)
        {
        case FilterPattern::SimpleExistence:
            return FilterPatternEvaluator<FilterPattern::SimpleExistence>::eval(ctx, tk, v);
        case FilterPattern::SimpleComparison:
            return FilterPatternEvaluator<FilterPattern::SimpleComparison>::eval(ctx, tk, v);
        case FilterPattern::NumericComparison:
            return FilterPatternEvaluator<FilterPattern::NumericComparison>::eval(ctx, tk, v);
        case FilterPattern::LengthComparison:
            return FilterPatternEvaluator<FilterPattern::LengthComparison>::eval(ctx, tk, v);
        case FilterPattern::ContextReference:
        case FilterPattern::Generic:
        default:
            // Fall back to generic embedded filter evaluation
            return std::unexpected(EvalError::TypeMismatchObject);
        }
    }
};

} // namespace json_query::json_path::internal
