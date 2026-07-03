// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include <QString>
#include <QJsonValue>
#include <functional>
#include <optional>
#include <vector>
#include <ctre.hpp>

#include "json-query/json-path/JSONPathFilter.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path::detail
{

// Enum to characterize comparison types for template specialization
enum class ComparisonType
{
    Numeric,
    Boolean,
    Null,
    String,
    DeepEquality
};

// Template functions for type-specific comparisons
template <ComparisonType Type>
bool compareValue(const QJsonValue& v, const QString& op, const auto& rhs) = delete;

// Explicit specializations
template <>
bool compareValue<ComparisonType::Numeric>(const QJsonValue& v, const QString& op, const double& numVal);

template <>
bool compareValue<ComparisonType::Boolean>(const QJsonValue& v, const QString& op, const bool& boolVal);

template <>
bool compareValue<ComparisonType::Null>(const QJsonValue& v, const QString& op, const std::nullptr_t&);

template <>
bool compareValue<ComparisonType::String>(const QJsonValue& v, const QString& op, const QString& rhs);

template <>
bool compareValue<ComparisonType::DeepEquality>(const QJsonValue& v, const QString& op, const QString& rhs);

template <>
bool compareValue<ComparisonType::String>(const QJsonValue& v, const QString& op, const QString& strVal);

template <>
bool compareValue<ComparisonType::DeepEquality>(const QJsonValue& v, const QString& op, const QJsonValue& rhsVal);

// Helper function for JSON value comparison logic with RFC 9535 semantics
bool performComparison(const QJsonValue& leftVal, const QString& op, const QJsonValue& rightVal);

// Lightweight comparison context using template dispatch
struct ComparisonContext
{
    QString        op;
    QString        rhs;
    double         numVal{0.0};
    bool           boolVal{false};
    ComparisonType type;
    bool           rhsQuoted{false};

    // Template-based comparison dispatch
    bool compare(const QJsonValue& v) const;
};

// Monadic helper to parse and classify RHS values, eliminating if-else cascades
[[nodiscard]] std::optional<ComparisonContext> parseRhsValue(const QString& op, QString rhs);

// Monadic operator dispatch helper to eliminate repetitive if-else chains
template <typename T>
bool applyOperator(const QString& op, const T& left, const T& right);

// Specialized operator dispatch for floating point numbers with fuzzy comparison
template <>
bool applyOperator<double>(const QString& op, const double& left, const double& right);

// Specialized operator dispatch for booleans with custom ordering (false < true)
template <>
bool applyOperator<bool>(const QString& op, const bool& left, const bool& right);

// ============================================================================
// Comparison Dispatcher Infrastructure (was JSONPathFilterComparisonDispatcher.hpp)
// ============================================================================

// Enum representing all comparison filter types with priority ordering
enum class ComparisonFilterType
{
    // Null comparisons (highest priority - most specific)
    NullPropertyDot,     // @.prop == null
    NullPropertyBracket, // @["key"] == null
    NullArrayIndex,      // @[index] == null

    // Self comparisons (high priority)
    DirectSelf,          // @ == @
    SelfPropertyDot,     // @.prop == @
    SelfPropertyBracket, // @["key"] == @
    SelfArrayIndex,      // @[index] == @
    SelfValue,           // @ == value

    // Property-to-property comparisons (medium priority)
    PropertyToProperty, // @.a == @.b
    PropertyToArray,    // @.prop == @.list[9]
    ArrayToProperty,    // @.list[9] == @.prop

    // Basic property comparisons (lowest priority - most general)
    BasicPropertyDot,     // @.prop == value
    BasicPropertyBracket, // @["key"] == value
    BasicArrayIndex       // @[index] == value
};

// Pattern definition template specializations (TableGen-style record definitions)
template <ComparisonFilterType Type>
struct ComparisonPatternDef
{
    static constexpr bool               enabled = false;
    static constexpr ctll::fixed_string pattern{""};
};

// Token factory template specializations (TableGen-style code generation)
template <ComparisonFilterType Type>
struct ComparisonTokenFactory
{
    static std::optional<Token> create(const QString& s, std::vector<FilterFn>& out)
    {
        return std::nullopt; // Default: not implemented
    }
};

// Variadic template dispatch table (TableGen-inspired compile-time dispatch)
template <ComparisonFilterType... Types>
struct ComparisonDispatchTable
{
    static std::optional<Token> dispatch(const QString& s, std::vector<FilterFn>& out);

  private:
    template <ComparisonFilterType First, ComparisonFilterType... Rest>
    static std::optional<Token> dispatchImpl(const QString& s, std::vector<FilterFn>& out);
};

// Complete dispatch table with priority ordering (most specific first)
using ComparisonDispatcher = ComparisonDispatchTable<
    // Null comparisons (highest priority)
    ComparisonFilterType::NullPropertyDot,
    ComparisonFilterType::NullPropertyBracket,
    ComparisonFilterType::NullArrayIndex,

    // Self comparisons (high priority)
    ComparisonFilterType::DirectSelf,
    ComparisonFilterType::SelfPropertyDot,
    ComparisonFilterType::SelfPropertyBracket,
    ComparisonFilterType::SelfArrayIndex,
    ComparisonFilterType::SelfValue,

    // Property-to-property comparisons (medium priority)
    ComparisonFilterType::PropertyToProperty,
    ComparisonFilterType::PropertyToArray,
    ComparisonFilterType::ArrayToProperty,

    // Basic property comparisons (lowest priority)
    ComparisonFilterType::BasicPropertyDot,
    ComparisonFilterType::BasicPropertyBracket,
    ComparisonFilterType::BasicArrayIndex>;

} // namespace json_query::json_path::detail
