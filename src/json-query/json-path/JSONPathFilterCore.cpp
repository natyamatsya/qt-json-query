#include "json-query/json-path/JSONPathFilterParsers.hpp"
#include "json-query/json-path/JSONPathFilterFunctions.hpp"
#include "json-query/json-path/JSONPathFilterHelpers.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include <QRegularExpression>
#include <QJsonDocument>
#include <ctre.hpp>

namespace json_query::json_path::detail {

using json_query::utils::to_sv;
using json_query::utils::to_qstr;
using json_query::json_path::FilterFn;
using json_query::json_path::Token;
using json_query::json_path::detail::splitTopLevel;

// Helper function to strip outer parentheses
QString stripOuterParens(QString s) {
    s = s.trimmed();
    while (s.startsWith('(') && s.endsWith(')')) {
        // Check if these are truly outer parentheses by counting
        int depth = 0;
        bool isOuter = true;
        for (int i = 1; i < s.length() - 1; ++i) {
            if (s[i] == '(') depth++;
            else if (s[i] == ')') {
                depth--;
                if (depth < 0) {
                    isOuter = false;
                    break;
                }
            }
        }
        if (isOuter && depth == 0) {
            s = s.mid(1, s.length() - 2).trimmed();
        } else {
            break;
        }
    }
    return s;
}

// Remaining parser function implementations
std::optional<Token> parseOr(QString s, QVector<FilterFn>& out)
{
    if (auto split = splitTopLevel(s, "||"_L1); split)
    {
        const auto& [lhsS, rhsS] = *split;

        QVector<FilterFn> tmp;
        auto lhsT = json_query::json_path::compileFilter(lhsS.trimmed(), tmp);
        auto rhsT = json_query::json_path::compileFilter(rhsS.trimmed(), tmp);
        if (!lhsT || !rhsT || tmp.isEmpty()) return std::nullopt;

        Builder b{out};
        FilterFn lhs = *(tmp.end()-2);
        FilterFn rhs = *(tmp.end()-1);
        return b.add([lhs, rhs](const QJsonValue& j){ return lhs(j) || rhs(j); });
    }
    return std::nullopt;
}

std::optional<Token> parseAnd(QString s, QVector<FilterFn>& out)
{
    if (auto split = splitTopLevel(s, "&&"_L1); split)
    {
        const auto& [lhsS, rhsS] = *split;

        QVector<FilterFn> tmp;
        auto lhsT = json_query::json_path::compileFilter(lhsS.trimmed(), tmp);
        auto rhsT = json_query::json_path::compileFilter(rhsS.trimmed(), tmp);
        if (!lhsT || !rhsT || tmp.isEmpty()) return std::nullopt;

        Builder b{out};
        FilterFn lhs = *(tmp.end()-2);
        FilterFn rhs = *(tmp.end()-1);
        return b.add([lhs, rhs](const QJsonValue& j){ return lhs(j) && rhs(j); });
    }
    return std::nullopt;
}

std::optional<Token> parseIn(QString s, QVector<FilterFn>& out)
{
    constexpr auto pat = ctll::fixed_string{
        R"('\s*([^']+?)\s*'\s+in\s+@\[['\"]([^'"]+)['\"]\])"};
    if (auto m = ctre::match<pat>(to_sv(s)))
    {
        const QString want  = to_qstr(m.template get<1>().to_view());
        const QString array = to_qstr(m.template get<2>().to_view());

        Builder b{out};
        return b.add([want, array](const QJsonValue& j){
            auto a = j[array];
            if (!a.isArray()) return false;
            
            // Use ContainerCursor for optimized, zero-copy array iteration during 'in' evaluation
            const QJsonArray arr = a.toArray();
            auto cursor = internal::ContainerCursor::array(arr);
            for (const auto& v : cursor) {
                if (v.isString() && v.toString() == want) return true;
            }
            return false;
        }, array);
    }
    return std::nullopt;
}

std::optional<Token> parseCompare(QString s, QVector<FilterFn>& out)
{
    constexpr auto dotPat = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=|>=|<=|>|<)\s*(.+))"};
    constexpr auto brkPat = ctll::fixed_string{R"(@\[['\"]([^'"]+)['\"]\]\s*(==|!=|>=|<=|>|<)\s*(.+))"};
    constexpr auto idxPat = ctll::fixed_string{R"(@\[(-?\d+)\]\s*(==|!=|>=|<=|>|<)\s*(.+))"};
    
    // Null comparison patterns
    constexpr auto dotNullPat = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=)\s*null)"};
    constexpr auto brkNullPat = ctll::fixed_string{R"(@\[['\"]([^'"]+)['\"]\]\s*(==|!=)\s*null)"};
    constexpr auto idxNullPat = ctll::fixed_string{R"(@\[(-?\d+)\]\s*(==|!=)\s*null)"};
    
    // Self comparison patterns
    constexpr auto dotSelfPat = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=|>=|<=|>|<)\s*@)"};
    constexpr auto brkSelfPat = ctll::fixed_string{R"(@\[['\"]([^'"]+)['\"]\]\s*(==|!=|>=|<=|>|<)\s*@)"};
    constexpr auto idxSelfPat = ctll::fixed_string{R"(@\[(-?\d+)\]\s*(==|!=|>=|<=|>|<)\s*@)"};
    
    // Property-to-property comparison patterns: @.a == @.b
    constexpr auto propToPropPat = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=|<|>|<=|>=)\s*@\.([\w$]+))"};
    
    // Property-to-property with array indexing: @.a == @.list[9] or @.list[9] == @.a
    constexpr auto propToArrayPat = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=|<|>|<=|>=)\s*@\.([\w$]+)\[(-?\d+)\])"};
    constexpr auto arrayToPropPat = ctll::fixed_string{R"(@\.([\w$]+)\[(-?\d+)\]\s*(==|!=|<|>|<=|>=)\s*@\.([\w$]+))"};
    
    // Direct self-comparison pattern: @==@ or @!=@
    constexpr auto directSelfPat = ctll::fixed_string{R"(^@\s*(==|!=|>=|<=|>|<)\s*@$)"};
    
    // Direct self-comparison with value pattern: @ == value
    constexpr auto selfValuePat = ctll::fixed_string{R"(^@\s*(==|!=|>=|<=|>|<)\s*(.+)$)"};

    // Try null comparisons first (more specific)
    if (auto t = parseNullCompare<dotNullPat>(s, out)) return t;
    if (auto t = parseNullCompare<brkNullPat>(s, out)) return t;
    if (auto t = parseNullCompareIndex<idxNullPat>(s, out)) return t;
    
    // Try self comparisons
    // Direct self-comparison first (most specific)
    if (ctre::match<directSelfPat>(to_sv(s))) {
        auto m = ctre::match<directSelfPat>(to_sv(s));
        const QString op = to_qstr(m.template get<1>().to_view());
        
        Builder b{out};
        return b.add([op](const QJsonValue& j){
            // Direct self-comparison: @ == @ is always true, @ != @ is always false
            // For ordering operators, self-comparison is always false
            if (op == "==") return true;
            if (op == "!=") return false;
            return false;
        }, QString("@%1@").arg(op));
    }

    if (auto t = parseSelfCompare<dotSelfPat>(s, out)) return t;
    if (auto t = parseSelfCompare<brkSelfPat>(s, out)) return t;
    if (auto t = parseSelfCompareIndex<idxSelfPat>(s, out)) return t;

    // Try direct self-comparison with value
    if (auto t = parseSelfValue<selfValuePat>(s, out)) return t;

    // Try property-to-property comparisons
    if (ctre::match<propToPropPat>(to_sv(s))) {
        auto m = ctre::match<propToPropPat>(to_sv(s));
        const QString leftProp = to_qstr(m.template get<1>().to_view());
        const QString op = to_qstr(m.template get<2>().to_view());
        const QString rightProp = to_qstr(m.template get<3>().to_view());
        
        Builder b{out};
        return b.add([leftProp, op, rightProp](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto leftVal = obj.value(leftProp);
            const auto rightVal = obj.value(rightProp);
            
            // Handle missing properties as null for comparison
            if (leftVal.type() == QJsonValue::Undefined || rightVal.type() == QJsonValue::Undefined) {
                if (op == "==") return leftVal.type() == rightVal.type(); // both undefined
                if (op == "!=") return leftVal.type() != rightVal.type(); // one undefined, one not
                return false; // ordering comparisons require same type
            }
            
            // Use deep equality for == and !=
            if (op == "==") return leftVal == rightVal;
            if (op == "!=") return leftVal != rightVal;
            
            // For ordering comparisons, ensure same type
            if (leftVal.type() != rightVal.type()) return false;
            
            // Handle ordering comparisons by type
            if (leftVal.isDouble() && rightVal.isDouble()) {
                double left = leftVal.toDouble();
                double right = rightVal.toDouble();
                if (op == "<") return left < right;
                if (op == ">") return left > right;
                if (op == "<=") return left <= right;
                if (op == ">=") return left >= right;
            } else if (leftVal.isBool() && rightVal.isBool()) {
                bool left = leftVal.toBool();
                bool right = rightVal.toBool();
                if (op == "<") return !left && right;  // false < true
                if (op == ">") return left && !right;  // true > false
                if (op == "<=") return !left || right; // false <= anything, true <= true
                if (op == ">=") return left || !right; // true >= anything, false >= false
            } else if (leftVal.isString() && rightVal.isString()) {
                QString left = leftVal.toString();
                QString right = rightVal.toString();
                if (op == "<") return left < right;
                if (op == ">") return left > right;
                if (op == "<=") return left <= right;
                if (op == ">=") return left >= right;
            }
            
            return false; // unsupported comparison
        }, QString("%1%2%3").arg(leftProp, op, rightProp));
    }

    // Try property-to-array comparisons
    if (ctre::match<propToArrayPat>(to_sv(s))) {
        auto m = ctre::match<propToArrayPat>(to_sv(s));
        const QString leftProp = to_qstr(m.template get<1>().to_view());
        const QString op = to_qstr(m.template get<2>().to_view());
        const QString rightProp = to_qstr(m.template get<3>().to_view());
        const QString rightIndex = to_qstr(m.template get<4>().to_view());
        
        Builder b{out};
        return b.add([leftProp, op, rightProp, rightIndex](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto leftVal = obj.value(leftProp);
            const auto rightArr = obj.value(rightProp).toArray();
            
            bool ok;
            int idx = rightIndex.toInt(&ok);
            QJsonValue rightVal;
            
            // Handle out-of-bounds or invalid index as undefined
            if (!ok || idx < 0 || idx >= rightArr.size()) {
                rightVal = QJsonValue(QJsonValue::Undefined);
            } else {
                rightVal = rightArr.at(idx);
            }
            
            // Handle missing properties and out-of-bounds indices as undefined for comparison
            if (leftVal.type() == QJsonValue::Undefined || rightVal.type() == QJsonValue::Undefined) {
                if (op == "==") return leftVal.type() == rightVal.type(); // both undefined
                if (op == "!=") return leftVal.type() != rightVal.type(); // one undefined, one not
                return false; // ordering comparisons require same type
            }
            
            // Use deep equality for == and !=
            if (op == "==") return leftVal == rightVal;
            if (op == "!=") return leftVal != rightVal;
            
            // For ordering comparisons, ensure same type
            if (leftVal.type() != rightVal.type()) return false;
            
            // Handle ordering comparisons by type
            if (leftVal.isDouble() && rightVal.isDouble()) {
                double left = leftVal.toDouble();
                double right = rightVal.toDouble();
                if (op == "<") return left < right;
                if (op == ">") return left > right;
                if (op == "<=") return left <= right;
                if (op == ">=") return left >= right;
            } else if (leftVal.isBool() && rightVal.isBool()) {
                bool left = leftVal.toBool();
                bool right = rightVal.toBool();
                if (op == "<") return !left && right;
                if (op == ">") return left && !right;
                if (op == "<=") return !left || right;
                if (op == ">=") return left || !right;
            } else if (leftVal.isString() && rightVal.isString()) {
                QString left = leftVal.toString();
                QString right = rightVal.toString();
                if (op == "<") return left < right;
                if (op == ">") return left > right;
                if (op == "<=") return left <= right;
                if (op == ">=") return left >= right;
            }
            
            return false;
        }, QString("%1%2%3[%4]").arg(leftProp, op, rightProp, rightIndex));
    }

    // Try array-to-property comparisons (reverse of above)
    if (ctre::match<arrayToPropPat>(to_sv(s))) {
        auto m = ctre::match<arrayToPropPat>(to_sv(s));
        const QString leftProp = to_qstr(m.template get<1>().to_view());
        const QString leftIndex = to_qstr(m.template get<2>().to_view());
        const QString op = to_qstr(m.template get<3>().to_view());
        const QString rightProp = to_qstr(m.template get<4>().to_view());
        
        Builder b{out};
        return b.add([leftProp, leftIndex, op, rightProp](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto leftArr = obj.value(leftProp).toArray();
            const auto rightVal = obj.value(rightProp);
            
            bool ok;
            int idx = leftIndex.toInt(&ok);
            QJsonValue leftVal;
            
            // Handle out-of-bounds or invalid index as undefined
            if (!ok || idx < 0 || idx >= leftArr.size()) {
                leftVal = QJsonValue(QJsonValue::Undefined);
            } else {
                leftVal = leftArr.at(idx);
            }
            
            // Handle missing properties and out-of-bounds indices as undefined for comparison
            if (leftVal.type() == QJsonValue::Undefined || rightVal.type() == QJsonValue::Undefined) {
                if (op == "==") return leftVal.type() == rightVal.type(); // both undefined
                if (op == "!=") return leftVal.type() != rightVal.type(); // one undefined, one not
                return false; // ordering comparisons require same type
            }
            
            // Use deep equality for == and !=
            if (op == "==") return leftVal == rightVal;
            if (op == "!=") return leftVal != rightVal;
            
            // For ordering comparisons, ensure same type
            if (leftVal.type() != rightVal.type()) return false;
            
            // Handle ordering comparisons by type
            if (leftVal.isDouble() && rightVal.isDouble()) {
                double left = leftVal.toDouble();
                double right = rightVal.toDouble();
                if (op == "<") return left < right;
                if (op == ">") return left > right;
                if (op == "<=") return left <= right;
                if (op == ">=") return left >= right;
            } else if (leftVal.isBool() && rightVal.isBool()) {
                bool left = leftVal.toBool();
                bool right = rightVal.toBool();
                if (op == "<") return !left && right;
                if (op == ">") return left && !right;
                if (op == "<=") return !left || right;
                if (op == ">=") return left || !right;
            } else if (leftVal.isString() && rightVal.isString()) {
                QString left = leftVal.toString();
                QString right = rightVal.toString();
                if (op == "<") return left < right;
                if (op == ">") return left > right;
                if (op == "<=") return left <= right;
                if (op == ">=") return left >= right;
            }
            
            return false;
        }, QString("%1[%2]%3%4").arg(leftProp, leftIndex, op, rightProp));
    }

    // Try basic property comparisons using template dispatch
    if (auto t = parseCompare1<dotPat>(s, out)) return t;
    if (auto t = parseCompare1<brkPat>(s, out)) return t;
    if (auto t = parseCompareIndex<idxPat>(s, out)) return t;
    
    return std::nullopt;
}

std::optional<Token> parseRegex(QString s, QVector<FilterFn>& out)
{
    constexpr auto dotPat = ctll::fixed_string{R"(@\.([\w$]+)\s*=~\s*/(.+)/)"};
    constexpr auto brkPat = ctll::fixed_string{R"(@\[['\"]([^'"]+)['\"]\]\s*=~\s*/(.+)/)"};

    if (auto t = parseRegex1<dotPat>(s, out)) return t;
    return        parseRegex1<brkPat>(s, out);
}

// Forward-declare the individual parsers
using RuleFn = std::optional<Token>(*)(QString, QVector<FilterFn>&);

constexpr std::array rules = {
    &parseOr,      // lowest precedence first
    &parseAnd,
    &parseNot,     // Add negation parser with high precedence
    &parseAbsolutePath, // Add absolute path parser
    &parseIn,
    &parseExists,
    &parseSelfCmp,
    &parseFunction, // Add function call parser
    &parseCompare,
    &parseRegex
};

} // namespace json_query::json_path::detail

// Table-driven dispatch
namespace json_query::json_path {

// Public dispatcher
std::optional<Token> compileFilter(const QString& expr, QVector<FilterFn>& out)
{
    QString s = json_query::json_path::detail::stripOuterParens(expr);
    qCDebug(jsonPathLog) << "compileFilter expr=" << expr << "stripped=" << s;
    for (int i = 0; i < detail::rules.size(); ++i) {
        const auto rule = detail::rules[i];
        qCDebug(jsonPathLog) << "Trying rule" << i;
        if (auto result = rule(s, out)) {
            qCDebug(jsonPathLog) << "compileFilter accepted token kind=" << (result ? static_cast<int>(result->kind) : -1) << "from rule" << i;
            return result;
        }
    }
    return std::nullopt;
}

} // namespace json_query::json_path
