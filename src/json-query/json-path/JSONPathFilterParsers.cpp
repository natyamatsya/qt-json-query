#include "json-query/json-path/JSONPathFilterParsers.hpp"
#include "json-query/json-path/JSONPathFilterComparison.hpp"
#include "json-query/json-path/JSONPathFilterHelpers.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <ctre.hpp>

namespace json_query::json_path::detail {

using json_query::utils::to_sv;
using json_query::utils::to_qstr;

// Individual parser function implementations (non-template)
// Note: parseOr, parseAnd, parseIn, parseCompare, parseRegex are implemented in JSONPathFilterCore.cpp

std::optional<json_query::json_path::Token> parseExists(QString s, QVector<json_query::json_path::FilterFn>& out)
{
    // Existence patterns for various JSONPath expressions
    constexpr auto dotPat = ctll::fixed_string{R"(^@\.([\w$]+)$)"};
    constexpr auto brkPat = ctll::fixed_string{R"(^@\[['\"]([^'"]+)['\"]\]$)"};
    constexpr auto rootPat = ctll::fixed_string{R"(^@$)"};
    constexpr auto wildcardPat = ctll::fixed_string{R"(^@\.\*$)"};
    
    // Root reference pattern for $[?$] - checks if root document exists
    constexpr auto rootRefPat = ctll::fixed_string{R"(^\$$)"};
    
    // Negated patterns
    constexpr auto negRootPat = ctll::fixed_string{R"(^!@$)"};
    constexpr auto negWildcardPat = ctll::fixed_string{R"(^!@\.\*$)"};
    constexpr auto negDotPat = ctll::fixed_string{R"(^!@\.([\w$]+)$)"};
    constexpr auto negBrkPat = ctll::fixed_string{R"(^!@\[['\"]([^'"]+)['\"]\]$)"};
    constexpr auto negRootRefPat = ctll::fixed_string{R"(^!\$$)"};
    
    // Array slice patterns for existence tests
    constexpr auto arraySlicePat = ctll::fixed_string{R"(^@\[(-?\d+):(-?\d+)\]$)"};
    constexpr auto negArraySlicePat = ctll::fixed_string{R"(^!@\[(-?\d+):(-?\d+)\]$)"};
    
    // Multi-selector existence patterns for tests like @[0, 0, 'a'] or @[1, 'key']
    // Exclude nested filter patterns that start with ? (those should be handled by nested filter patterns)
    constexpr auto multiSelectorPat = ctll::fixed_string{R"(^@\[([^?:][^:]*)\]$)"};
    constexpr auto negMultiSelectorPat = ctll::fixed_string{R"(^!@\[([^?:][^:]*)\]$)"};
    
    // Nested filter pattern for tests like @[?@>1] - apply filter to current array/object
    constexpr auto nestedFilterPat = ctll::fixed_string{R"(^@\[\?(.+)\]$)"};
    constexpr auto negNestedFilterPat = ctll::fixed_string{R"(^!@\[\?(.+)\]$)"};
    
    // Function to create existence test token
    auto makeExistenceToken = [&](const QString& prop) -> json_query::json_path::Token {
        Builder b{out};
        return b.add([prop](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            // RFC 9535: existence filters check for property presence, not truthiness
            // A property exists if it's present in the object, regardless of its value (including false, 0, "", [], {})
            // Only undefined (missing) properties are considered non-existent
            return v.type() != QJsonValue::Undefined;
        }, prop);
    };

    auto makeArrayIndexToken = [&](int index)->json_query::json_path::Token {
        Builder b{out};
        return b.add([index](const QJsonValue& j){
            if (!j.isArray()) return false; // non-arrays don't have indices
            const auto arr = j.toArray();
            if (index < 0 || index >= arr.size()) return false; // out of bounds is absent
            const auto& v = arr[index];
            // RFC 9535: existence filters check for element presence, not truthiness
            // An array element exists if it's within bounds, regardless of its value (including false, 0, "", [], {})
            return v.type() != QJsonValue::Undefined;
        }, QString("@[%1]").arg(index));
    };

    auto makeArraySliceToken = [&](int start, int end)->json_query::json_path::Token {
        Builder b{out};
        return b.add([start, end](const QJsonValue& j){
            if (!j.isArray()) return false; // non-arrays don't have slices
            const auto arr = j.toArray();
            int actualStart = start < 0 ? 0 : start;
            int actualEnd = end < 0 ? arr.size() : qMin(end, arr.size());
            // RFC 9535: existence filters check for element presence, not truthiness
            // A slice exists if it contains any elements within bounds, regardless of their values
            return actualStart < actualEnd && actualStart < arr.size();
        }, QString("@[%1:%2]").arg(start).arg(end));
    };

    auto makeRootToken = [&]()->json_query::json_path::Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            // RFC 9535: root existence filter checks if the root value exists
            // The root always exists unless it's explicitly undefined
            return j.type() != QJsonValue::Undefined;
        }, "@");
    };

    auto makeWildcardToken = [&]()->json_query::json_path::Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            // RFC 9535: wildcard existence filters check for element/property presence
            if (j.isObject()) {
                const auto obj = j.toObject();
                // Any property that exists (is not undefined) should match
                return !obj.empty(); // If object has any properties, wildcard existence is true
            } else if (j.isArray()) {
                const auto arr = j.toArray();
                // Any array element that exists should match
                return !arr.empty(); // If array has any elements, wildcard existence is true
            }
            return false; // Primitives have no properties or elements
        }, "@.*");
    };

    auto makeNegatedToken = [&](const QString& prop)->json_query::json_path::Token {
        Builder b{out};
        return b.add([prop](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            // RFC 9535: negated existence filters check for property absence, not falsy values
            // A property is absent only if it's undefined (missing from the object)
            return v.type() == QJsonValue::Undefined;
        }, "!" + prop);
    };

    auto makeNegatedArrayIndexToken = [&](int index)->json_query::json_path::Token {
        Builder b{out};
        return b.add([index](const QJsonValue& j){
            if (!j.isArray()) return true; // non-arrays don't have indices
            const auto arr = j.toArray();
            if (index < 0 || index >= arr.size()) return true; // out of bounds is absent
            const auto& v = arr[index];
            // RFC 9535: negated existence filters check for element absence
            return v.type() == QJsonValue::Undefined;
        }, QString("!@[%1]").arg(index));
    };

    auto makeNegatedArraySliceToken = [&](int start, int end)->json_query::json_path::Token {
        Builder b{out};
        return b.add([start, end](const QJsonValue& j){
            if (!j.isArray()) return true; // non-arrays don't have slices
            const auto arr = j.toArray();
            int actualStart = start < 0 ? 0 : start;
            int actualEnd = end < 0 ? arr.size() : qMin(end, arr.size());
            // Check if slice has NO elements within bounds (negated)
            return !(actualStart < actualEnd && actualStart < arr.size());
        }, QString("!@[%1:%2]").arg(start).arg(end));
    };

    auto makeNegatedRootToken = [&]()->json_query::json_path::Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            // RFC 9535: negated root existence filter checks if the root value is absent
            // The root is absent only if it's explicitly undefined
            return j.type() == QJsonValue::Undefined;
        }, "!@");
    };

    auto makeNegatedWildcardToken = [&]()->json_query::json_path::Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            // RFC 9535: negated wildcard existence filters check for element/property absence
            if (j.isObject()) {
                const auto obj = j.toObject();
                // If object has no properties, then !@.* is true
                return obj.empty();
            } else if (j.isArray()) {
                const auto arr = j.toArray();
                // If array has no elements, then !@.* is true
                return arr.empty();
            }
            return true; // Primitives have no properties or elements, so !@.* is true
        }, "!@.*");
    };

    auto makeMultiSelectorToken = [&](const QString& selectorsStr)->json_query::json_path::Token {
        Builder b{out};
        return b.add([selectorsStr](const QJsonValue& j){
            // Multi-selector existence test: check if any of the selectors can be applied to j
            // Parse the selectors string and check each one
            QStringList selectors = selectorsStr.split(',');
            for (const QString& selectorRaw : selectors) {
                QString selector = selectorRaw.trimmed();
                bool exists = false;
                
                // Check if it's a quoted string selector
                if ((selector.startsWith('"') && selector.endsWith('"')) || 
                    (selector.startsWith('\'') && selector.endsWith('\''))) {
                    QString key = selector.mid(1, selector.size()-2);
                    if (j.isObject()) {
                        const auto obj = j.toObject();
                        exists = obj.contains(key);
                    }
                }
                // Check if it's a numeric index selector
                else {
                    bool ok;
                    int index = selector.toInt(&ok);
                    if (ok && j.isArray()) {
                        const auto arr = j.toArray();
                        exists = (index >= 0 && index < arr.size()) || 
                                (index < 0 && (-index) <= arr.size());
                    }
                }
                
                if (exists) return true; // If any selector exists, return true
            }
            return false; // None of the selectors exist
        }, QString("@[%1]").arg(selectorsStr));
    };

    auto makeNegatedMultiSelectorToken = [&](const QString& selectorsStr)->json_query::json_path::Token {
        Builder b{out};
        return b.add([selectorsStr](const QJsonValue& j){
            // Negated multi-selector existence test: check if none of the selectors can be applied to j
            QStringList selectors = selectorsStr.split(',');
            for (const QString& selectorRaw : selectors) {
                QString selector = selectorRaw.trimmed();
                bool exists = false;
                
                // Check if it's a quoted string selector
                if ((selector.startsWith('"') && selector.endsWith('"')) || 
                    (selector.startsWith('\'') && selector.endsWith('\''))) {
                    QString key = selector.mid(1, selector.size()-2);
                    if (j.isObject()) {
                        const auto obj = j.toObject();
                        exists = obj.contains(key);
                    }
                }
                // Check if it's a numeric index selector
                else {
                    bool ok;
                    int index = selector.toInt(&ok);
                    if (ok && j.isArray()) {
                        const auto arr = j.toArray();
                        exists = (index >= 0 && index < arr.size()) || 
                                (index < 0 && (-index) <= arr.size());
                    }
                }
                
                if (exists) return false; // If any selector exists, negation is false
            }
            return true; // None of the selectors exist, so negation is true
        }, QString("!@[%1]").arg(selectorsStr));
    };

    auto makeNestedFilterToken = [&](const QString& filterExpr)->json_query::json_path::Token {
        Builder b{out};
        return b.add([filterExpr](const QJsonValue& j){
            if (!j.isArray()) {
                return false; // Nested filters only work on arrays
            }
            
            const auto arr = j.toArray();
            
            // Compile the inner filter expression
            QVector<json_query::json_path::FilterFn> innerFilterFns;
            auto innerToken = json_query::json_path::compileFilter(filterExpr, innerFilterFns);
            if (!innerToken || innerFilterFns.isEmpty()) {
                return false; // Failed to compile inner filter
            }
            
            json_query::json_path::FilterFn innerFilterFn = innerFilterFns.last();
            
            // Apply the inner filter to each array element
            for (int i = 0; i < arr.size(); ++i) {
                const auto& element = arr[i];
                bool matches = innerFilterFn(element);
                if (matches) {
                    return true; // Found at least one matching element
                }
            }
            return false; // No elements matched the inner filter
        }, QString("@[?%1]").arg(filterExpr));
    };

    auto makeNegatedNestedFilterToken = [&](const QString& filterExpr)->json_query::json_path::Token {
        Builder b{out};
        return b.add([filterExpr](const QJsonValue& j){
            // Negated nested filter existence test: compile and apply the inner filter to array elements
            if (!j.isArray()) {
                return true; // Non-arrays don't match nested filters, so negation is true
            }
            
            const auto arr = j.toArray();
            
            // Compile the inner filter expression
            QVector<json_query::json_path::FilterFn> innerFilterFns;
            auto innerToken = json_query::json_path::compileFilter(filterExpr, innerFilterFns);
            if (!innerToken || innerFilterFns.isEmpty()) {
                return true; // Failed to compile inner filter, so negation is true
            }
            
            json_query::json_path::FilterFn innerFilterFn = innerFilterFns.last();
            
            // Apply the inner filter to each array element
            for (int i = 0; i < arr.size(); ++i) {
                const auto& element = arr[i];
                bool matches = innerFilterFn(element);
                if (matches) {
                    return false; // Found a matching element, so negation is false
                }
            }
            return true; // No elements matched the inner filter, so negation is true
        }, QString("!@[?%1]").arg(filterExpr));
    };

    // Check negated patterns first (more specific)
    if (auto m = ctre::match<negRootPat>(to_sv(s)))
        return makeNegatedRootToken();
    if (auto m = ctre::match<negWildcardPat>(to_sv(s)))
        return makeNegatedWildcardToken();
    if (auto m = ctre::match<negArraySlicePat>(to_sv(s))) {
        QString startStr = to_qstr(m.template get<1>().to_view());
        QString endStr = to_qstr(m.template get<2>().to_view());
        int start = startStr.isEmpty() ? 0 : startStr.toInt();
        int end = endStr.isEmpty() ? -1 : endStr.toInt();
        return makeNegatedArraySliceToken(start, end);
    }
    if (auto m = ctre::match<negDotPat>(to_sv(s)))
        return makeNegatedToken(to_qstr(m.template get<1>().to_view()));
    if (auto m = ctre::match<negBrkPat>(to_sv(s)))
        return makeNegatedToken(to_qstr(m.template get<1>().to_view()));
    if (auto m = ctre::match<negRootRefPat>(to_sv(s)))
        return makeNegatedRootToken();

    // Root reference existence filter: $[?$] - always true (root document always exists)
    if (ctre::match<rootRefPat>(to_sv(s))) {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            // Root document always exists
            return true;
        }, QString("$"));
    }

    // Check for root existence filter
    if (auto m = ctre::match<rootPat>(to_sv(s)))
        return makeRootToken();
    
    // Check for wildcard existence filter
    if (auto m = ctre::match<wildcardPat>(to_sv(s)))
        return makeWildcardToken();
    
    // Check for array access patterns
    if (auto m = ctre::match<arraySlicePat>(to_sv(s))) {
        QString startStr = to_qstr(m.template get<1>().to_view());
        QString endStr = to_qstr(m.template get<2>().to_view());
        int start = startStr.isEmpty() ? 0 : startStr.toInt();
        int end = endStr.isEmpty() ? -1 : endStr.toInt();
        return makeArraySliceToken(start, end);
    }
    
    // RFC 9535: Support existence filters like $[?@.a] but reject incomplete predicates
    // The distinction is context-dependent and handled by the parsing order
    
    if (auto m = ctre::match<dotPat>(to_sv(s)))
        return makeExistenceToken(to_qstr(m.template get<1>().to_view()));
    if (auto m = ctre::match<brkPat>(to_sv(s)))
        return makeExistenceToken(to_qstr(m.template get<1>().to_view()));
    
    // Multi-selector existence patterns
    if (auto m = ctre::match<multiSelectorPat>(to_sv(s))) {
        QString selectorsStr = to_qstr(m.template get<1>().to_view());
        return makeMultiSelectorToken(selectorsStr);
    }
    if (auto m = ctre::match<negMultiSelectorPat>(to_sv(s))) {
        QString selectorsStr = to_qstr(m.template get<1>().to_view());
        return makeNegatedMultiSelectorToken(selectorsStr);
    }

    if (auto m = ctre::match<nestedFilterPat>(to_sv(s))) {
        QString filterExpr = to_qstr(m.template get<1>().to_view());
        return makeNestedFilterToken(filterExpr);
    }
    if (auto m = ctre::match<negNestedFilterPat>(to_sv(s))) {
        QString filterExpr = to_qstr(m.template get<1>().to_view());
        return makeNegatedNestedFilterToken(filterExpr);
    }
    return std::nullopt;
}

std::optional<json_query::json_path::Token> parseSelfCmp(QString s, QVector<json_query::json_path::FilterFn>& out)
{
    static constexpr auto pat = ctll::fixed_string{
        R"(^@\s*(==|!=|\>=|\<=|\>|\<)\s*@$)"};
    
    if (auto m = ctre::match<pat>(to_sv(s)))
    {
        const QString op = to_qstr(m.template get<1>().to_view());
        
        Builder b{out};
        return b.add([op](const QJsonValue& j){
            // Self-comparison: compare the value to itself
            // This is always true for == and always false for !=
            // For ordering operators, it depends on the value type
            if (op == "==") return true;
            if (op == "!=") return false;
            // For ordering operators, self-comparison is always false
            // (a value cannot be less than, greater than itself)
            return false;
        }, QString("@%1@").arg(op));
    }
    
    return std::nullopt;
}

std::optional<json_query::json_path::Token> parseNot(QString s, QVector<json_query::json_path::FilterFn>& out)
{
    constexpr auto negParenPat = ctll::fixed_string{R"(^!\s*\(\s*(.*)\s*\)$)"};
    if (auto m = ctre::match<negParenPat>(to_sv(s))) {
        QString innerExpr = to_qstr(m.template get<1>().to_view()).trimmed();
        QVector<json_query::json_path::FilterFn> innerFilters;
        if (auto innerToken = json_query::json_path::compileFilter(innerExpr, innerFilters)) {
            Builder b{out};
            return b.add([innerFilters, innerExpr](const QJsonValue& j) -> bool {
                if (!innerFilters.empty()) {
                    bool innerResult = innerFilters[0](j);
                    bool negatedResult = !innerResult;
                    return negatedResult;
                }
                return false;
            }, QString("!(%1)").arg(innerExpr));
        }
    }
    if (s.startsWith('!') && s.length() > 1) {
        QString innerExpr = s.mid(1).trimmed();
        QVector<json_query::json_path::FilterFn> innerFilters;
        if (auto innerToken = json_query::json_path::compileFilter(innerExpr, innerFilters)) {
            Builder b{out};
            return b.add([innerFilters, innerExpr](const QJsonValue& j) -> bool {
                if (!innerFilters.empty()) {
                    bool innerResult = innerFilters[0](j);
                    bool negatedResult = !innerResult;
                    return negatedResult;
                }
                return false;
            }, QString("!%1").arg(innerExpr));
        }
    }
    return std::nullopt;
}

std::optional<json_query::json_path::Token> parseAbsolutePath(QString s, QVector<json_query::json_path::FilterFn>& out)
{
    // Check if expression starts with $ (absolute path reference)
    if (!s.startsWith('$')) {
        return std::nullopt;
    }
    
    // RFC 9535: Reject expressions that contain comparison operators
    // These should be handled by other filter rules, not absolute path parsing
    if (s.contains("==") || s.contains("!=") || s.contains("<=") || s.contains(">=") || 
        s.contains("<") || s.contains(">") || s.contains("&&") || s.contains("||")) {
        return std::nullopt;
    }
    
    // Only accept simple absolute path references like $, $.foo, $.*.a, etc.
    // Not complex expressions or comparisons
    using json_query::JSONPath;
    
    // Try to create the JSONPath - if it fails, the pattern is invalid
    auto testPath = JSONPath::create(s);
    if (!testPath) {
        return std::nullopt; // Invalid absolute path pattern
    }
    
    Builder b{out};
    return b.add([s](const QJsonValue& rootValue) -> bool {
        // Create a temporary JSONPath to evaluate the absolute path
        // against the root document
        if (auto path = JSONPath::create(s)) {
            auto results = path->evaluateAll(rootValue);
            if (results) {
                return !results->isEmpty();
            }
            return false;
        }
        return false;
    }, s);
}

} // namespace json_query::json_path::detail
