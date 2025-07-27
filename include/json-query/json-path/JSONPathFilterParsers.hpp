#pragma once

#include <QString>
#include <QVector>
#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <optional>
#include <functional>
#include <vector>
#include "json-query/json-path/JSONPathFilterComparison.hpp"
#include "json-query/json-path/JSONPathFilterHelpers.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include <ctre.hpp>

namespace json_query::json_path {
    using FilterFn = std::function<bool(const QJsonValue&)>;
    struct Token;
}

namespace json_query::json_path::detail {

using json_query::utils::to_sv;
using json_query::utils::to_qstr;

// Forward declarations
class FilterParseContext;

// Template implementations for comparison patterns - must be in header for templates
template<auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token> parseSelfValue(QString s, std::vector<json_query::json_path::FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const auto op = to_qstr(match.template get<1>().to_view());
        const auto value = to_qstr(match.template get<2>().to_view());
        
        return parseRhsValue(op, value)
            .and_then([&](const ComparisonContext& ctx) -> std::optional<json_query::json_path::Token> {
                Builder b{out};
                
                // Use monadic dispatch based on comparison type
                switch (ctx.type) {
                    case ComparisonType::Numeric:
                        return b.add([op = ctx.op, numVal = ctx.numVal](const QJsonValue& j) {
                            return j.isDouble() && applyOperator(op, j.toDouble(), numVal);
                        }, QString("@"));
                        
                    case ComparisonType::Boolean:
                        return b.add([op = ctx.op, boolVal = ctx.boolVal](const QJsonValue& j) {
                            return j.isBool() && applyOperator(op, j.toBool(), boolVal);
                        }, QString("@"));
                        
                    case ComparisonType::Null:
                        return b.add([op = ctx.op](const QJsonValue& j) {
                            auto isJNull{j.isNull()};
                            // Null only supports equality comparisons
                            if (op == "==") return isJNull;
                            if (op == "!=") return !isJNull;
                            return false; // null doesn't support ordering comparisons
                        }, QString("@"));
                        
                    case ComparisonType::String:
                    case ComparisonType::DeepEquality:
                        return b.add([op = ctx.op, value = ctx.rhs](const QJsonValue& j) {
                            return j.isString() && applyOperator(op, j.toString(), value);
                        }, QString("@"));
                }
                return std::nullopt;
            });
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token> parseCompare1(QString s, std::vector<json_query::json_path::FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const auto prop = to_qstr(match.template get<1>().to_view());
        const auto op = to_qstr(match.template get<2>().to_view());
        const auto rhs = to_qstr(match.template get<3>().to_view());
        
        return parseRhsValue(op, rhs)
            .and_then([&](const ComparisonContext& ctx) -> std::optional<json_query::json_path::Token> {
                Builder b{out};
                return b.add([prop, ctx](const QJsonValue& j){
                    const auto obj{j.toObject()};
                    const auto v{obj.value(prop)};
                    return ctx.compare(v);
                }, prop);
            });
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token> parseCompareIndex(QString s, std::vector<json_query::json_path::FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const auto prop = to_qstr(match.template get<1>().to_view());
        const auto op = to_qstr(match.template get<2>().to_view());
        const auto rhs = to_qstr(match.template get<3>().to_view());
        
        return parseRhsValue(op, rhs)
            .and_then([&](const ComparisonContext& ctx) -> std::optional<json_query::json_path::Token> {
                Builder b{out};
                return b.add([prop, ctx](const QJsonValue& j){
                    // Convert prop to integer for array index access
                    bool ok;
                    auto index{prop.toInt(&ok)};
                    if (!ok) return false; // Invalid index
                    
                    // Array index access: only works on arrays, not objects
                    if (j.isArray()) {
                        const auto arr{j.toArray()};
                        if (index < 0 || index >= arr.size()) {
                            // Out of bounds: compare with undefined/null
                            QJsonValue undefined; // QJsonValue::Undefined
                            return ctx.compare(undefined);
                        } else {
                            const auto v{arr[index]};
                            return ctx.compare(v);
                        }
                    } else {
                        // Non-arrays don't have array indices: compare with undefined/null
                        QJsonValue undefined; // QJsonValue::Undefined
                        return ctx.compare(undefined);
                    }
                }, QString("@[%1]").arg(prop));
            });
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token> parseRegex1(QString s, std::vector<json_query::json_path::FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const auto prop = to_qstr(match.template get<1>().to_view());
        auto pattern = to_qstr(match.template get<2>().to_view());
        
        if (!unquote(pattern)) return std::nullopt;
        
        // REQUIRED: QRegularExpression for user-provided runtime regex patterns
        const QRegularExpression regex(pattern);
        if (!regex.isValid()) return std::nullopt;
        
        return Builder{out}.add([prop, regex](const QJsonValue& j){
            const auto obj{j.toObject()};
            const auto v{obj.value(prop)};
            if (!v.isString()) return false;
            return regex.match(v.toString()).hasMatch();
        }, prop);
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token> parseSelfCompare(QString s, std::vector<json_query::json_path::FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const auto op = to_qstr(match.template get<1>().to_view());
        const auto rhs = to_qstr(match.template get<2>().to_view());
        
        return parseRhsValue(op, rhs)
            .and_then([&](const ComparisonContext& ctx) -> std::optional<json_query::json_path::Token> {
                Builder b{out};
                
                // Use monadic dispatch based on comparison type
                switch (ctx.type) {
                    case ComparisonType::Numeric:
                        return b.add([op = ctx.op, numVal = ctx.numVal](const QJsonValue& j) {
                            return j.isDouble() && applyOperator(op, j.toDouble(), numVal);
                        }, QString("@"));
                        
                    case ComparisonType::Boolean:
                        return b.add([op = ctx.op, boolVal = ctx.boolVal](const QJsonValue& j) {
                            return j.isBool() && applyOperator(op, j.toBool(), boolVal);
                        }, QString("@"));
                        
                    case ComparisonType::Null:
                        return b.add([op = ctx.op](const QJsonValue& j) {
                            auto isJNull{j.isNull()};
                            // Null only supports equality comparisons
                            if (op == "==") return isJNull;
                            if (op == "!=") return !isJNull;
                            return false; // null doesn't support ordering comparisons
                        }, QString("@"));
                        
                    case ComparisonType::String:
                    case ComparisonType::DeepEquality:
                        return b.add([op = ctx.op, value = ctx.rhs](const QJsonValue& j) {
                            return j.isString() && applyOperator(op, j.toString(), value);
                        }, QString("@"));
                }
                return std::nullopt;
            });
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token> parseSelfCompareIndex(QString s, std::vector<json_query::json_path::FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const auto op = to_qstr(match.template get<1>().to_view());
        const auto rhs = to_qstr(match.template get<2>().to_view());
        
        return parseRhsValue(op, rhs)
            .and_then([&](const ComparisonContext& ctx) -> std::optional<json_query::json_path::Token> {
                Builder b{out};
                
                // Use monadic dispatch based on comparison type
                switch (ctx.type) {
                    case ComparisonType::Numeric:
                        return b.add([op = ctx.op, numVal = ctx.numVal](const QJsonValue& j) {
                            return j.isDouble() && applyOperator(op, j.toDouble(), numVal);
                        }, QString("@"));
                        
                    case ComparisonType::Boolean:
                        return b.add([op = ctx.op, boolVal = ctx.boolVal](const QJsonValue& j) {
                            return j.isBool() && applyOperator(op, j.toBool(), boolVal);
                        }, QString("@"));
                        
                    case ComparisonType::Null:
                        return b.add([op = ctx.op](const QJsonValue& j) {
                            auto isJNull{j.isNull()};
                            // Null only supports equality comparisons
                            if (op == "==") return isJNull;
                            if (op == "!=") return !isJNull;
                            return false; // null doesn't support ordering comparisons
                        }, QString("@"));
                        
                    case ComparisonType::String:
                    case ComparisonType::DeepEquality:
                        return b.add([op = ctx.op, value = ctx.rhs](const QJsonValue& j) {
                            return j.isString() && applyOperator(op, j.toString(), value);
                        }, QString("@"));
                }
                return std::nullopt;
            });
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token> parseNullCompare(QString s, std::vector<json_query::json_path::FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const auto prop = to_qstr(match.template get<1>().to_view());
        const auto op = to_qstr(match.template get<2>().to_view());
        
        ComparisonContext ctx;
        ctx.op = op;
        ctx.rhs = "null";
        ctx.type = ComparisonType::Null;
        
        return Builder{out}.add([prop, ctx](const QJsonValue& j){
            const auto obj{j.toObject()};
            const auto v{obj.value(prop)};
            return ctx.compare(v);
        }, prop);
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token> parseNullCompareIndex(QString s, std::vector<json_query::json_path::FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const auto prop = to_qstr(match.template get<1>().to_view());
        const auto op = to_qstr(match.template get<2>().to_view());
        
        ComparisonContext ctx;
        ctx.op = op;
        ctx.rhs = "null";
        ctx.type = ComparisonType::Null;
        
        return Builder{out}.add([prop, ctx](const QJsonValue& j){
            // Convert prop to integer for array index access
            bool ok;
            auto index{prop.toInt(&ok)};
            if (!ok) return false; // Invalid index
            
            // Array index access: only works on arrays, not objects
            if (j.isArray()) {
                const auto arr{j.toArray()};
                if (index < 0 || index >= arr.size()) {
                    // Out of bounds: compare with undefined/null
                    QJsonValue undefined; // QJsonValue::Undefined
                    return ctx.compare(undefined);
                } else {
                    const auto v{arr[index]};
                    return ctx.compare(v);
                }
            } else {
                // Non-arrays don't have array indices: compare with undefined/null
                QJsonValue undefined; // QJsonValue::Undefined
                return ctx.compare(undefined);
            }
        }, QString("@[%1]").arg(prop));
    }
    return std::nullopt;
}

// Individual parser functions
std::optional<json_query::json_path::Token> parseOr(const QString& s, std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseAnd(const QString& s, std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseIn(const QString& s, std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseCompare(const QString& s, std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseRegex(const QString& s, std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseExists(const QString& s, std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseSelfCmp(const QString& s, std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseNot(const QString& s, std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseAbsolutePath(const QString& s, std::vector<json_query::json_path::FilterFn>& out);

// ──────────────────────────────────────────────────────────────────────
//  Modern Embedded Filter Parser Functions (Zero-Overhead)
// ──────────────────────────────────────────────────────────────────────

// Template implementations for embedded filter patterns - must be in header for templates
template<ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedCompare1(const QString& s)
{
    if (auto m = ctre::match<Pattern>(to_sv(s))) {
        const auto prop = to_qstr(m.template get<1>().to_view());
        const auto op = to_qstr(m.template get<2>().to_view());
        const auto rhs = to_qstr(m.template get<3>().to_view());
        
        // Parse RHS value using existing comparison context logic
        auto ctx{parseRhsValue(op, rhs)};
        if (!ctx) return std::nullopt;
        
        Token token;
        token.kind = Token::Kind::Filter;
        token.key = s;
        
        // Embed filter directly into token
        token.embedFilter([prop, ctx = *ctx](const QJsonValue& j) {
            const auto obj{j.toObject()};
            const auto v{obj.value(prop)};
            return ctx.compare(v);
        });
        
        return token;
    }
    return std::nullopt;
}

template<ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedCompareIndex(const QString& s)
{
    if (auto m = ctre::match<Pattern>(to_sv(s))) {
        const auto indexStr = to_qstr(m.template get<1>().to_view());
        const auto op = to_qstr(m.template get<2>().to_view());
        const auto rhs = to_qstr(m.template get<3>().to_view());
        
        // Parse RHS value using existing comparison context logic
        auto ctx{parseRhsValue(op, rhs)};
        if (!ctx) return std::nullopt;
        
        bool ok;
        auto index{indexStr.toInt(&ok)};
        if (!ok) return std::nullopt;
        
        Token token;
        token.kind = Token::Kind::Filter;
        token.key = s;
        
        // Embed filter directly into token
        token.embedFilter([index, ctx = *ctx](const QJsonValue& j) {
            if (!j.isArray()) return false;
            const auto arr{j.toArray()};
            if (index < 0 || index >= arr.size()) return false;
            const auto v{arr[index]};
            return ctx.compare(v);
        });
        
        return token;
    }
    return std::nullopt;
}

template<ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedSelfValue(const QString& s)
{
    if (auto m = ctre::match<Pattern>(to_sv(s))) {
        const auto op = to_qstr(m.template get<1>().to_view());
        const auto rhs = to_qstr(m.template get<2>().to_view());
        
        // Parse RHS value using existing comparison context logic
        auto ctx{parseRhsValue(op, rhs)};
        if (!ctx) return std::nullopt;
        
        Token token;
        token.kind = Token::Kind::Filter;
        token.key = s;
        
        // Embed filter directly into token
        token.embedFilter([ctx = *ctx](const QJsonValue& j) {
            return ctx.compare(j);
        });
        
        return token;
    }
    return std::nullopt;
}

template<ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedRegex1(const QString& s)
{
    if (auto m = ctre::match<Pattern>(to_sv(s))) {
        const auto prop = to_qstr(m.template get<1>().to_view());
        const auto pattern = to_qstr(m.template get<2>().to_view());
        
        // Create regex object
        QRegularExpression regex(pattern);
        if (!regex.isValid()) return std::nullopt;
        
        Token token;
        token.kind = Token::Kind::Filter;
        token.key = s;
        
        // Embed regex filter directly into token
        token.embedFilter([prop, regex](const QJsonValue& j) {
            const auto obj{j.toObject()};
            const auto v{obj.value(prop)};
            if (!v.isString()) return false;
            return regex.match(v.toString()).hasMatch();
        });
        
        return token;
    }
    return std::nullopt;
}

// Individual embedded filter parser functions
std::optional<Token> parseEmbeddedOr(const QString& s);
std::optional<Token> parseEmbeddedAnd(const QString& s);
std::optional<Token> parseEmbeddedIn(const QString& s);
std::optional<Token> parseEmbeddedCompare(const QString& s);
std::optional<Token> parseEmbeddedRegex(const QString& s);
std::optional<Token> parseEmbeddedExists(const QString& s);
std::optional<Token> parseEmbeddedSelfCmp(const QString& s);
std::optional<Token> parseEmbeddedNot(const QString& s);

} // namespace detail
