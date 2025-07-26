#include "json-query/json-path/JSONPathFilterParsers.hpp"
#include "json-query/json-path/JSONPathFilterHelpers.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include <QRegularExpression>
#include <ctre.hpp>

namespace json_query::json_path::detail {

using json_query::utils::to_sv;
using json_query::utils::to_qstr;
using json_query::json_path::FilterFn;
using json_query::json_path::Token;

// Large parser function implementations that couldn't fit in the main file

std::optional<Token> parseExists(QString s, QVector<FilterFn>& out)
{
    // Existence patterns for various JSONPath expressions
    constexpr auto rootPat = ctll::fixed_string{R"(^\$$)"};
    constexpr auto wildcardPat = ctll::fixed_string{R"(^@\.\*$)"};
    constexpr auto dotPat = ctll::fixed_string{R"(^@\.([a-zA-Z_][a-zA-Z0-9_]*)$)"};
    constexpr auto brkPat = ctll::fixed_string{R"(^@\[([^\]]+)\]$)"};
    constexpr auto arraySlicePat = ctll::fixed_string{R"(^@\[(-?\d*):(-?\d*)\]$)"};
    constexpr auto multiSelectorPat = ctll::fixed_string{R"(^@\[([^\]]+(?:,[^\]]+)*)\]$)"};
    constexpr auto nestedFilterPat = ctll::fixed_string{R"(^@\[\?(.+)\]$)"};
    constexpr auto rootRefPat = ctll::fixed_string{R"(^\$$)"};
    
    // Negated patterns
    constexpr auto negRootPat = ctll::fixed_string{R"(^!\$$)"};
    constexpr auto negWildcardPat = ctll::fixed_string{R"(^!@\.\*$)"};
    constexpr auto negDotPat = ctll::fixed_string{R"(^!@\.([a-zA-Z_][a-zA-Z0-9_]*)$)"};
    constexpr auto negBrkPat = ctll::fixed_string{R"(^!@\[([^\]]+)\]$)"};
    constexpr auto negArraySlicePat = ctll::fixed_string{R"(^!@\[(-?\d*):(-?\d*)\]$)"};
    constexpr auto negMultiSelectorPat = ctll::fixed_string{R"(^!@\[([^\]]+(?:,[^\]]+)*)\]$)"};
    constexpr auto negNestedFilterPat = ctll::fixed_string{R"(^!@\[\?(.+)\]$)"};
    constexpr auto negRootRefPat = ctll::fixed_string{R"(^!\$$)"};

    // Helper lambda functions for token creation
    auto makeRootToken = [&]()->Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            return true; // Root document always exists
        }, QString("$"));
    };

    auto makeNegatedRootToken = [&]()->Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            return false; // Root document always exists, so negation is always false
        }, QString("!$"));
    };

    auto makeWildcardToken = [&]()->Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            // Wildcard exists if the value is an object or array with at least one element
            if (j.isObject()) return !j.toObject().isEmpty();
            if (j.isArray()) return !j.toArray().isEmpty();
            return false;
        }, QString("@.*"));
    };

    auto makeNegatedWildcardToken = [&]()->Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            // Negated wildcard: true if the value is not an object/array or is empty
            if (j.isObject()) return j.toObject().isEmpty();
            if (j.isArray()) return j.toArray().isEmpty();
            return true; // Non-containers don't have wildcard children
        }, QString("!@.*"));
    };

    auto makeExistenceToken = [&](const QString& prop)->Token {
        Builder b{out};
        return b.add([prop](const QJsonValue& j){
            // Check if it's a quoted string property
            QString actualProp = prop;
            if ((prop.startsWith('"') && prop.endsWith('"')) || 
                (prop.startsWith('\'') && prop.endsWith('\''))) {
                actualProp = prop.mid(1, prop.size()-2);
                if (j.isObject()) {
                    return j.toObject().contains(actualProp);
                }
            }
            // Check if it's a numeric index
            else {
                bool ok;
                int index = prop.toInt(&ok);
                if (ok && j.isArray()) {
                    const auto arr = j.toArray();
                    return (index >= 0 && index < arr.size()) || 
                           (index < 0 && (-index) <= arr.size());
                }
                // Also check as object property for unquoted strings
                else if (j.isObject()) {
                    return j.toObject().contains(prop);
                }
            }
            return false;
        }, QString("@[%1]").arg(prop));
    };

    auto makeNegatedToken = [&](const QString& prop)->Token {
        Builder b{out};
        return b.add([prop](const QJsonValue& j){
            // Negated existence test
            QString actualProp = prop;
            if ((prop.startsWith('"') && prop.endsWith('"')) || 
                (prop.startsWith('\'') && prop.endsWith('\''))) {
                actualProp = prop.mid(1, prop.size()-2);
                if (j.isObject()) {
                    return !j.toObject().contains(actualProp);
                }
            }
            else {
                bool ok;
                int index = prop.toInt(&ok);
                if (ok && j.isArray()) {
                    const auto arr = j.toArray();
                    return !((index >= 0 && index < arr.size()) || 
                            (index < 0 && (-index) <= arr.size()));
                }
                else if (j.isObject()) {
                    return !j.toObject().contains(prop);
                }
            }
            return true; // Non-matching types don't have the property
        }, QString("!@[%1]").arg(prop));
    };

    // Check negated patterns first (more specific)
    if (auto m = ctre::match<negRootPat>(to_sv(s)))
        return makeNegatedRootToken();
    if (auto m = ctre::match<negWildcardPat>(to_sv(s)))
        return makeNegatedWildcardToken();
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
    
    // RFC 9535: Support existence filters like $[?@.a] but reject incomplete predicates
    // The distinction is context-dependent and handled by the parsing order
    
    if (auto m = ctre::match<dotPat>(to_sv(s)))
        return makeExistenceToken(to_qstr(m.template get<1>().to_view()));
    if (auto m = ctre::match<brkPat>(to_sv(s)))
        return makeExistenceToken(to_qstr(m.template get<1>().to_view()));
    
    return std::nullopt;
}

std::optional<Token> parseNot(QString s, QVector<FilterFn>& out)
{
    // Handle negation with parentheses: !(...) 
    constexpr auto negParenPat = ctll::fixed_string{R"(^!\s*\(\s*(.*)\s*\)$)"};
    
    if (auto m = ctre::match<negParenPat>(to_sv(s)))
    {
        QString innerExpr = to_qstr(m.template get<1>().to_view()).trimmed();
        
        // Recursively parse the inner expression
        QVector<FilterFn> innerFilters;
        if (auto innerToken = json_query::json_path::compileFilter(innerExpr, innerFilters))
        {
            // Create a negated version of the inner filter
            Builder b{out};
            return b.add([innerFilters](const QJsonValue& j) -> bool {
                // Apply the inner filter and negate the result
                if (!innerFilters.empty()) {
                    return !innerFilters[0](j);
                }
                return false;
            }, QString("!(%1)").arg(innerExpr));
        }
    }
    
    // Handle simple negation: !@.prop, !@['prop'], etc.
    if (s.startsWith('!') && s.length() > 1)
    {
        QString innerExpr = s.mid(1).trimmed();
        
        // Recursively parse the inner expression
        QVector<FilterFn> innerFilters;
        if (auto innerToken = json_query::json_path::compileFilter(innerExpr, innerFilters))
        {
            // Create a negated version of the inner filter
            Builder b{out};
            return b.add([innerFilters](const QJsonValue& j) -> bool {
                // Apply the inner filter and negate the result
                if (!innerFilters.empty()) {
                    return !innerFilters[0](j);
                }
                return false;
            }, QString("!%1").arg(innerExpr));
        }
    }
    
    return std::nullopt;
}

std::optional<Token> parseAbsolutePath(QString s, QVector<FilterFn>& out)
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
