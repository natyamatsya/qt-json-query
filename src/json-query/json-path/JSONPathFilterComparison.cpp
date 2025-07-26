#include "json-query/json-path/JSONPathFilterComparison.hpp"
#include "json-query/json-path/JSONPathFilterHelpers.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtCore/qmath.h>

namespace json_query::json_path::detail {

// Template specializations for type-specific comparisons
template<>
bool compareValue<ComparisonType::Numeric>(const QJsonValue& v, const QString& op, const double& numVal)
{
    if (!v.isDouble()) {
        // RFC 9535: Cross-type comparisons - different types are never equal
        if (op == "==") return false;
        if (op == "!=") return true;
        return false;
    }
    const double val = v.toDouble();
    
    if (op == "==") return qFuzzyCompare(val, numVal);
    if (op == "!=") return !qFuzzyCompare(val, numVal);
    if (op == "<")  return val < numVal;
    if (op == ">")  return val > numVal;
    if (op == "<=") return val <= numVal;
    if (op == ">=") return val >= numVal;
    return false;
}

template<>
bool compareValue<ComparisonType::Boolean>(const QJsonValue& v, const QString& op, const bool& boolVal)
{
    if (!v.isBool()) {
        // RFC 9535: Cross-type comparisons - different types are never equal
        if (op == "==") return false;
        if (op == "!=") return true;
        return false;
    }
    const bool val = v.toBool();
    
    if (op == "==") return val == boolVal;
    if (op == "!=") return val != boolVal;
    // Boolean ordering: false < true
    if (op == "<")  return !val && boolVal;
    if (op == ">")  return val && !boolVal;
    if (op == "<=") return !val || boolVal;
    if (op == ">=") return val || !boolVal;
    return false;
}

template<>
bool compareValue<ComparisonType::Null>(const QJsonValue& v, const QString& op, const std::nullptr_t&)
{
    if (!v.isNull()) {
        if (op == "==") return false;
        if (op == "!=") return true;
        return false;
    }
    
    // RFC 9535: null ordering comparisons
    if (op == "==") return true;
    if (op == "!=") return false;
    if (op == "<=") return true;
    if (op == ">=") return true;
    return false; // < and > are false for null
}

template<>
bool compareValue<ComparisonType::String>(const QJsonValue& v, const QString& op, const QString& rhs)
{
    if (!v.isString()) {
        // RFC 9535: Cross-type comparisons - different types are never equal
        if (op == "==") return false;
        if (op == "!=") return true;
        return false;
    }
    const QString val = v.toString();
    
    if (op == "==") return val == rhs;
    if (op == "!=") return val != rhs;
    if (op == "<")  return val < rhs;
    if (op == ">")  return val > rhs;
    if (op == "<=") return val <= rhs;
    if (op == ">=") return val >= rhs;
    return false;
}

template<>
bool compareValue<ComparisonType::DeepEquality>(const QJsonValue& v, const QString& op, const QString& rhs)
{
    // Parse RHS as JSON for deep comparison
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(rhs.toUtf8(), &error);
    if (error.error != QJsonParseError::NoError) {
        return false; // Invalid JSON
    }
    
    const QJsonValue rhsValue = doc.isArray() ? QJsonValue(doc.array()) : QJsonValue(doc.object());
    if (op == "==") return v == rhsValue;
    if (op == "!=") return v != rhsValue;
    return false; // Deep equality only supports == and !=
}

// ComparisonContext implementation
bool ComparisonContext::compare(const QJsonValue& v) const
{
    switch (type) {
        case ComparisonType::Numeric:
            return compareValue<ComparisonType::Numeric>(v, op, numVal);
        case ComparisonType::Boolean:
            return compareValue<ComparisonType::Boolean>(v, op, boolVal);
        case ComparisonType::Null:
            return compareValue<ComparisonType::Null>(v, op, nullptr);
        case ComparisonType::String:
            return compareValue<ComparisonType::String>(v, op, rhs);
        case ComparisonType::DeepEquality:
            return compareValue<ComparisonType::DeepEquality>(v, op, rhs);
    }
    return false;
}

// Monadic helper to parse and classify RHS values, eliminating if-else cascades
[[nodiscard]] std::optional<ComparisonContext> parseRhsValue(const QString& op, QString rhs)
{
    const bool isNum = isValidJsonNumber(rhs);
    const bool isBool = (rhs == "true" || rhs == "false");
    const bool isNull = (rhs == "null");
    
    // Check if RHS is quoted (for string vs deep equality distinction)
    bool rhsQuoted = false;
    if ((rhs.startsWith('"') && rhs.endsWith('"')) || 
        (rhs.startsWith('\'') && rhs.endsWith('\''))) {
        rhsQuoted = true;
    }
    
    // Parse values using functional composition instead of if-else cascade
    auto parseNumeric = [&]() -> std::optional<ComparisonContext> {
        if (!isNum) return std::nullopt;
        return ComparisonContext{op, rhs, rhs.toDouble(), false, ComparisonType::Numeric, rhsQuoted};
    };
    
    auto parseBoolean = [&]() -> std::optional<ComparisonContext> {
        if (!isBool) return std::nullopt;
        return ComparisonContext{op, rhs, 0.0, (rhs == "true"), ComparisonType::Boolean, rhsQuoted};
    };
    
    auto parseNull = [&]() -> std::optional<ComparisonContext> {
        if (!isNull) return std::nullopt;
        return ComparisonContext{op, rhs, 0.0, false, ComparisonType::Null, rhsQuoted};
    };
    
    auto parseString = [&]() -> std::optional<ComparisonContext> {
        QString processedRhs = rhs;
        if (!isNum && !isBool && !isNull) {
            // Only accept quoted strings or valid unquoted literals
            if (!rhsQuoted) {
                // Reject invalid unquoted literals like "Null", "True", "False", etc.
                // Only allow valid JSON literals (null, true, false, numbers) or quoted strings
                return std::nullopt;
            }
            (void)unquote(processedRhs);
        }
        ComparisonType type = rhsQuoted ? ComparisonType::String : ComparisonType::DeepEquality;
        return ComparisonContext{op, processedRhs, 0.0, false, type, rhsQuoted};
    };
    
    // Monadic chaining to try each parser in priority order
    return parseNumeric().or_else(parseBoolean).or_else(parseNull).or_else(parseString);
}

// Monadic operator dispatch helper to eliminate repetitive if-else chains
template<typename T>
bool applyOperator(const QString& op, const T& left, const T& right)
{
    if (op == "==") return left == right;
    if (op == "!=") return left != right;
    if (op == "<") return left < right;
    if (op == ">") return left > right;
    if (op == "<=") return left <= right;
    if (op == ">=") return left >= right;
    return false;
}

// Specialized operator dispatch for floating point numbers with fuzzy comparison
template<>
bool applyOperator<double>(const QString& op, const double& left, const double& right)
{
    if (op == "==") return qFuzzyCompare(left, right);
    if (op == "!=") return !qFuzzyCompare(left, right);
    if (op == "<") return left < right;
    if (op == ">") return left > right;
    if (op == "<=") return left <= right;
    if (op == ">=") return left >= right;
    return false;
}

// Specialized operator dispatch for booleans with custom ordering (false < true)
template<>
bool applyOperator<bool>(const QString& op, const bool& left, const bool& right)
{
    if (op == "==") return left == right;
    if (op == "!=") return left != right;
    // Custom boolean ordering: false < true
    if (op == "<") return !left && right;
    if (op == ">") return left && !right;
    if (op == "<=") return !left || !right || left == right;
    if (op == ">=") return left || !right || left == right;
    return false;
}

// Specialized operator dispatch for QString with string comparison
template<>
bool applyOperator<QString>(const QString& op, const QString& left, const QString& right)
{
    if (op == "==") return left == right;
    if (op == "!=") return left != right;
    if (op == "<") return left < right;
    if (op == ">") return left > right;
    if (op == "<=") return left <= right;
    if (op == ">=") return left >= right;
    return false;
}

} // namespace json_query::json_path::detail
