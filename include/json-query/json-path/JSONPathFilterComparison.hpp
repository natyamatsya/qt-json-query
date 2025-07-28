// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <QString>
#include <QJsonValue>
#include <optional>

namespace json_query::json_path::detail
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

} // namespace json_query::json_path::detail
