#include "json-query/json-path/JSONPathFilter.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"

#include <QDebug>
#include "json-query/json-path/JSONPathLog.hpp"
#include <iostream>

namespace json_query::json_path::detail {

// Enum to characterize comparison types for template specialization
enum class ComparisonType {
    Numeric,
    Boolean, 
    Null,
    String,
    DeepEquality
};

// Template functions for type-specific comparisons
template<ComparisonType Type>
bool compareValue(const QJsonValue& v, const QString& op, const auto& rhs) = delete;

template<>
bool compareValue<ComparisonType::Numeric>(const QJsonValue& v, const QString& op, const double& numVal) {
    // RFC 9535: strict type checking - no coercion between strings and numbers
    if (!v.isDouble()) {
        // Non-numeric values are not equal to numbers, but can be != 
        if (op == "==") return false;
        if (op == "!=") return true;
        return false; // ordering comparisons require same type
    }
    const double x = v.toDouble();
    if (op=="==") return x == numVal;
    if (op=="!=") return x != numVal;
    if (op=="<")  return x < numVal;
    if (op==">")  return x > numVal;
    if (op=="<=") return x <= numVal;
    if (op==">=") return x >= numVal;
    return false;
}

template<>
bool compareValue<ComparisonType::Boolean>(const QJsonValue& v, const QString& op, const bool& boolVal) {
    // RFC 9535: strict type checking - no coercion between booleans and other types
    if (!v.isBool()) {
        // Non-boolean values are not equal to booleans, but can be != 
        if (op == "==") return false;
        if (op == "!=") return true;
        return false; // ordering comparisons require same type
    }
    const bool b = v.toBool();
    if (op=="==") return b == boolVal;
    if (op=="!=") return b != boolVal;
    if (op=="<")  return !b && boolVal;  // false < true
    if (op==">")  return b && !boolVal;  // true > false
    if (op=="<=") return !b || boolVal;  // false <= anything, true <= true
    if (op==">=") return b || !boolVal;  // true >= anything, false >= false
    return false;
}

template<>
bool compareValue<ComparisonType::Null>(const QJsonValue& v, const QString& op, const std::nullptr_t&) {
    if (op=="==") return v.isNull();
    if (op=="!=") return !v.isNull();
    if (op=="<=") return v.isNull(); // null <= null is true
    if (op==">=") return v.isNull(); // null >= null is true
    return false; // null < null and null > null are false
}

template<>
bool compareValue<ComparisonType::String>(const QJsonValue& v, const QString& op, const QString& rhs) {
    const QString vs = v.toString();
    if (op=="==") return vs == rhs;
    if (op=="!=") return vs != rhs;
    if (op=="<")  return vs < rhs;
    if (op==">")  return vs > rhs;
    if (op=="<=") return vs <= rhs;
    if (op==">=") return vs >= rhs;
    return false;
}

template<>
bool compareValue<ComparisonType::DeepEquality>(const QJsonValue& v, const QString& op, const QString& rhs) {
    // Parse RHS as JSON for potential array/object comparison
    QJsonParseError parseError;
    QJsonDocument rhsDoc = QJsonDocument::fromJson(rhs.toUtf8(), &parseError);
    if (parseError.error == QJsonParseError::NoError) {
        QJsonValue rhsValue = rhsDoc.isArray() ? QJsonValue(rhsDoc.array()) : 
                             rhsDoc.isObject() ? QJsonValue(rhsDoc.object()) : QJsonValue();
        if (!rhsValue.isNull()) {
            if (op=="==") return v == rhsValue;
            if (op=="!=") return v != rhsValue;
            // Arrays and objects don't support ordering comparisons
            return false;
        }
    }
    // Fallback to string comparison
    return compareValue<ComparisonType::String>(v, op, rhs);
}

// Lightweight comparison context using template dispatch
struct ComparisonContext {
    QString op;
    QString rhs;
    double numVal = 0.0;
    bool boolVal = false;
    ComparisonType type;
    bool rhsQuoted = false;
    
    // Template-based comparison dispatch
    bool compare(const QJsonValue& v) const {
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
};

    using FilterFn = json_query::json_path::FilterFn;
    using Kind  = Token::Kind;
    using json_query::utils::to_sv;
    using json_query::utils::to_qstr;
    using json_query::json_path::detail::splitTopLevel;
    using json_query::json_path::detail::stripOuterParens;

// ───────────────────────────────────────────────────────────────
//  Helper functions and structures for filter compilation
// ───────────────────────────────────────────────────────────────

// Helper function for unquoting strings
[[nodiscard]] inline bool unquote(QString& s)
{
    if (s.size() < 2) return false;
    const QChar openQuote = s.front();
    const QChar closeQuote = s.back();
    
    // RFC 9535: Support mixed quote styles - opening can be ' or ", closing can be ' or "
    if ((openQuote != u'"' && openQuote != u'\'') || (closeQuote != u'"' && closeQuote != u'\'')) 
        return false;

    // For mixed quotes, we need to find the actual closing quote
    // Scan forward to find the closing quote that is NOT preceded by an odd
    // number of backslashes (i.e. it is truly terminating the literal).
    int idx = -1;
    for (int i = 1; i < s.size(); ++i) {
        if (s[i] != closeQuote) continue;
        // count preceding backslashes
        int backslashes = 0;
        int k = i - 1;
        while (k >=0 && s[k] == u'\\') { ++backslashes; --k; }
        if ((backslashes % 2) == 0) { idx = i; break; }
    }
    if (idx == -1) {
        // Fallback: assume last char is quote even if escaped; drop first and last
        if (s.back() != closeQuote) return false;
        s = s.mid(1, s.size()-2);
    } else {
        // Extract up to found idx
        s = s.mid(1, idx - 1);
    }

    // RFC 9535-compliant escape sequence processing (Table 4)
    QString result;
    result.reserve(s.size());
    
    for (int i = 0; i < s.size(); ++i) {
        if (s[i] == u'\\' && i + 1 < s.size()) {
            const QChar next = s[i + 1];
            switch (next.unicode()) {
                case u'b':  result += u'\b'; i++; break;  // backspace
                case u't':  result += u'\t'; i++; break;  // horizontal tab
                case u'n':  result += u'\n'; i++; break;  // line feed
                case u'f':  result += u'\f'; i++; break;  // form feed
                case u'r':  result += u'\r'; i++; break;  // carriage return
                case u'"':  result += u'"';  i++; break;  // quotation mark
                case u'\'': result += u'\''; i++; break;  // apostrophe
                case u'/':  result += u'/';  i++; break;  // solidus
                case u'\\': result += u'\\'; i++; break;  // reverse solidus
                case u'u':  // Unicode escape \uXXXX
                    if (i + 5 < s.size()) {
                        bool ok = false;
                        const QString hexStr = s.mid(i + 2, 4);
                        const ushort codePoint = hexStr.toUShort(&ok, 16);
                        if (ok) {
                            result += QChar(codePoint);
                            i += 5;  // skip \uXXXX
                        } else {
                            // Invalid Unicode escape, keep as-is
                            result += s[i];
                        }
                    } else {
                        // Incomplete Unicode escape, keep as-is
                        result += s[i];
                    }
                    break;
                default:
                    // Unknown escape sequence, keep as-is (RFC 9535 behavior)
                    result += s[i];
                    break;
            }
        } else {
            result += s[i];
        }
    }
    
    s = result;
    return true;
}

// A tiny façade so every parser can push a predicate and
// immediately obtain the corresponding Token.
struct Builder {
    QVector<FilterFn>& fns;

    [[nodiscard]] Token add(FilterFn fn, QString key = {})
    {
        fns.push_back(std::move(fn));
        const std::size_t id = fns.size() - 1;
        return Token{ Kind::Filter, 0, {}, 0u, std::move(key), id };
    }
};

// Template implementations for comparison patterns
template<auto PAT>
[[nodiscard]] std::optional<Token> parseCompare1(QString s, QVector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const QString prop = to_qstr(match.template get<1>().to_view());
        const QString op = to_qstr(match.template get<2>().to_view());
        QString rhs = to_qstr(match.template get<3>().to_view());
        
        const bool isNum = rhs.contains(QRegularExpression(R"(^-?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?$)"));
        const bool isBool = (rhs == "true" || rhs == "false");
        const bool isNull = (rhs == "null");
        const bool rhsQuoted = (rhs.startsWith('"') && rhs.endsWith('"')) || 
                              (rhs.startsWith('\'') && rhs.endsWith('\''));
        
        double numVal = 0.0;
        bool boolVal = false;
        
        if (isNum) {
            numVal = rhs.toDouble();
        } else if (isBool) {
            boolVal = (rhs == "true");
        } else if (!isNull && !rhsQuoted) {
            return std::nullopt;
        }
        
        if (!isNum && !isBool && !isNull)
            (void)unquote(rhs);
        
        ComparisonContext ctx;
        ctx.op = op;
        ctx.rhs = rhs;
        ctx.type = isNum ? ComparisonType::Numeric : 
                   isBool ? ComparisonType::Boolean : 
                   isNull ? ComparisonType::Null : 
                   rhsQuoted ? ComparisonType::DeepEquality : ComparisonType::String;
        ctx.numVal = numVal;
        ctx.boolVal = boolVal;
        ctx.rhsQuoted = rhsQuoted;
        
        Builder b{out};
        return b.add([prop, ctx](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            return ctx.compare(v);
        }, prop);
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<Token> parseCompareIndex(QString s, QVector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const QString prop = to_qstr(match.template get<1>().to_view());
        const QString op = to_qstr(match.template get<2>().to_view());
        QString rhs = to_qstr(match.template get<3>().to_view());
        
        const bool isNum = rhs.contains(QRegularExpression(R"(^-?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?$)"));
        const bool isBool = (rhs == "true" || rhs == "false");
        const bool isNull = (rhs == "null");
        const bool rhsQuoted = (rhs.startsWith('"') && rhs.endsWith('"')) || 
                              (rhs.startsWith('\'') && rhs.endsWith('\''));
        
        double numVal = 0.0;
        bool boolVal = false;
        
        if (isNum) {
            numVal = rhs.toDouble();
        } else if (isBool) {
            boolVal = (rhs == "true");
        } else if (!isNull && !rhsQuoted) {
            return std::nullopt;
        }
        
        if (!isNum && !isBool && !isNull)
            (void)unquote(rhs);
        
        ComparisonContext ctx;
        ctx.op = op;
        ctx.rhs = rhs;
        ctx.type = isNum ? ComparisonType::Numeric : 
                   isBool ? ComparisonType::Boolean : 
                   isNull ? ComparisonType::Null : 
                   rhsQuoted ? ComparisonType::DeepEquality : ComparisonType::String;
        ctx.numVal = numVal;
        ctx.boolVal = boolVal;
        ctx.rhsQuoted = rhsQuoted;
        
        Builder b{out};
        return b.add([prop, ctx](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            return ctx.compare(v);
        }, prop);
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<Token> parseNullCompare(QString s, QVector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const QString prop = to_qstr(match.template get<1>().to_view());
        const QString op = to_qstr(match.template get<2>().to_view());
        
        ComparisonContext ctx;
        ctx.op = op;
        ctx.rhs = "null";
        ctx.type = ComparisonType::Null;
        
        Builder b{out};
        return b.add([prop, ctx](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            return ctx.compare(v);
        }, prop);
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<Token> parseNullCompareIndex(QString s, QVector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const QString prop = to_qstr(match.template get<1>().to_view());
        const QString op = to_qstr(match.template get<2>().to_view());
        
        ComparisonContext ctx;
        ctx.op = op;
        ctx.rhs = "null";
        ctx.type = ComparisonType::Null;
        
        Builder b{out};
        return b.add([prop, ctx](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            return ctx.compare(v);
        }, prop);
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<Token> parseSelfCompare(QString s, QVector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const QString op = to_qstr(match.template get<1>().to_view());
        QString rhs = to_qstr(match.template get<2>().to_view());
        
        const bool isNum = rhs.contains(QRegularExpression(R"(^-?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?$)"));
        const bool isBool = (rhs == "true" || rhs == "false");
        const bool isNull = (rhs == "null");
        const bool rhsQuoted = (rhs.startsWith('"') && rhs.endsWith('"')) || 
                              (rhs.startsWith('\'') && rhs.endsWith('\''));
        
        double numVal = 0.0;
        bool boolVal = false;
        
        if (isNum) {
            numVal = rhs.toDouble();
        } else if (isBool) {
            boolVal = (rhs == "true");
        } else if (!isNull && !rhsQuoted) {
            return std::nullopt;
        }
        
        if (!isNum && !isBool && !isNull)
            (void)unquote(rhs);
        
        ComparisonContext ctx;
        ctx.op = op;
        ctx.rhs = rhs;
        ctx.type = isNum ? ComparisonType::Numeric : 
                   isBool ? ComparisonType::Boolean : 
                   isNull ? ComparisonType::Null : 
                   rhsQuoted ? ComparisonType::DeepEquality : ComparisonType::String;
        ctx.numVal = numVal;
        ctx.boolVal = boolVal;
        ctx.rhsQuoted = rhsQuoted;
        
        Builder b{out};
        return b.add([ctx](const QJsonValue& j){
            return ctx.compare(j);
        }, QString("@"));
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<Token> parseSelfCompareIndex(QString s, QVector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const QString op = to_qstr(match.template get<1>().to_view());
        QString rhs = to_qstr(match.template get<2>().to_view());
        
        const bool isNum = rhs.contains(QRegularExpression(R"(^-?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?$)"));
        const bool isBool = (rhs == "true" || rhs == "false");
        const bool isNull = (rhs == "null");
        const bool rhsQuoted = (rhs.startsWith('"') && rhs.endsWith('"')) || 
                              (rhs.startsWith('\'') && rhs.endsWith('\''));
        
        double numVal = 0.0;
        bool boolVal = false;
        
        if (isNum) {
            numVal = rhs.toDouble();
        } else if (isBool) {
            boolVal = (rhs == "true");
        } else if (!isNull && !rhsQuoted) {
            return std::nullopt;
        }
        
        if (!isNum && !isBool && !isNull)
            (void)unquote(rhs);
        
        ComparisonContext ctx;
        ctx.op = op;
        ctx.rhs = rhs;
        ctx.type = isNum ? ComparisonType::Numeric : 
                   isBool ? ComparisonType::Boolean : 
                   isNull ? ComparisonType::Null : 
                   rhsQuoted ? ComparisonType::DeepEquality : ComparisonType::String;
        ctx.numVal = numVal;
        ctx.boolVal = boolVal;
        ctx.rhsQuoted = rhsQuoted;
        
        Builder b{out};
        return b.add([ctx](const QJsonValue& j){
            return ctx.compare(j);
        }, QString("@"));
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<Token> parseRegex1(QString s, QVector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const QString prop = to_qstr(match.template get<1>().to_view());
        QString pattern = to_qstr(match.template get<2>().to_view());
        
        if (!unquote(pattern)) return std::nullopt;
        
        const QRegularExpression regex(pattern);
        if (!regex.isValid()) return std::nullopt;
        
        Builder b{out};
        return b.add([prop, regex](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            if (!v.isString()) return false;
            return regex.match(v.toString()).hasMatch();
        }, prop);
    }
    return std::nullopt;
}

template<auto PAT>
[[nodiscard]] std::optional<Token> parseSelfValue(QString s, QVector<FilterFn>& out)
{
    if (const auto match = ctre::match<PAT>(to_sv(s))) {
        const QString op = to_qstr(match.template get<1>().to_view());
        QString value = to_qstr(match.template get<2>().to_view());
        
        const bool isNum = value.contains(QRegularExpression(R"(^-?(0|[1-9]\d*)(\.\d+)?([eE][+-]?\d+)?$)"));
        const bool isBool = (value == "true" || value == "false");
        const bool isNull = (value == "null");
        const bool isQuoted = (value.startsWith('"') && value.endsWith('"')) || 
                             (value.startsWith('\'') && value.endsWith('\''));
        
        double numVal = 0.0;
        bool boolVal = false;
        
        if (isNum) {
            numVal = value.toDouble();
        } else if (isBool) {
            boolVal = (value == "true");
        } else if (!isNull && !isQuoted) {
            return std::nullopt;
        }
        
        if (!isNum && !isBool && !isNull)
            (void)unquote(value);
        
        Builder b{out};
        if (isNum) {
            return b.add([op, numVal](const QJsonValue& j){
                if (!j.isDouble()) return false;
                double jVal = j.toDouble();
                if (op == "==") return qFuzzyCompare(jVal, numVal);
                if (op == "!=") return !qFuzzyCompare(jVal, numVal);
                if (op == "<") return jVal < numVal;
                if (op == ">") return jVal > numVal;
                if (op == "<=") return jVal <= numVal;
                if (op == ">=") return jVal >= numVal;
                return false;
            }, QString("@"));
        } else if (isBool) {
            return b.add([op, boolVal](const QJsonValue& j){
                if (!j.isBool()) return false;
                bool jVal = j.toBool();
                if (op == "==") return jVal == boolVal;
                if (op == "!=") return jVal != boolVal;
                // Boolean ordering: false < true
                if (op == "<") return !jVal && boolVal;
                if (op == ">") return jVal && !boolVal;
                if (op == "<=") return !jVal || boolVal;
                if (op == ">=") return jVal || !boolVal;
                return false;
            }, QString("@"));
        } else if (isNull) {
            return b.add([op](const QJsonValue& j){
                bool isJNull = j.isNull();
                if (op == "==") return isJNull;
                if (op == "!=") return !isJNull;
                return false; // null doesn't support ordering comparisons
            }, QString("@"));
        } else {
            return b.add([op, value](const QJsonValue& j){
                if (!j.isString()) return false;
                QString jVal = j.toString();
                if (op == "==") return jVal == value;
                if (op == "!=") return jVal != value;
                if (op == "<") return jVal < value;
                if (op == ">") return jVal > value;
                if (op == "<=") return jVal <= value;
                if (op == ">=") return jVal >= value;
                return false;
            }, QString("@"));
        }
    }
    return std::nullopt;
}

// ───────────────────────────────────────────────────────────────
//  compileFilter  — turns [? …] into Token{Filter,…} + lambda
//      Supports three forms:
//        1.  @.prop  <op>  value          (numeric or string, == != > < >= <=)
//        2.  @['prop'] <op> value         (same operators)
//        3.  'foo' in @['arrayProp']
//      Extend with more patterns as needed.
// ────────────────── parser implementations ───────────────────────────
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
            for (const auto& v : a.toArray())
                if (v.isString() && v.toString() == want) return true;
            return false;
        }, array);
    }
    return std::nullopt;
}

std::optional<Token> parseCompare(QString s, QVector<FilterFn>& out)
{
    constexpr auto dotPat = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=|>=|<=|>|<)\s*(.+))"};
    constexpr auto brkPat = ctll::fixed_string{R"(@\[['\"]([^'\"]+)['\"]\]\s*(==|!=|>=|<=|>|<)\s*(.+))"};
    constexpr auto idxPat = ctll::fixed_string{R"(@\[(-?\d+)\]\s*(==|!=|>=|<=|>|<)\s*(.+))"};
    
    // Null comparison patterns
    constexpr auto dotNullPat = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=)\s*null)"};
    constexpr auto brkNullPat = ctll::fixed_string{R"(@\[['\"]([^'\"]+)['\"]\]\s*(==|!=)\s*null)"};
    constexpr auto idxNullPat = ctll::fixed_string{R"(@\[(-?\d+)\]\s*(==|!=)\s*null)"};
    
    // Self comparison patterns
    constexpr auto dotSelfPat = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=)\s*@)"};
    constexpr auto brkSelfPat = ctll::fixed_string{R"(@\[['\"]([^'\"]+)['\"]\]\s*(==|!=)\s*@)"};
    constexpr auto idxSelfPat = ctll::fixed_string{R"(@\[(-?\d+)\]\s*(==|!=)\s*@)"};
    
    // Property-to-property comparison patterns: @.a == @.b
    constexpr auto propToPropPat = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=|<|>|<=|>=)\s*@\.([\w$]+))"};
    
    // Direct self-comparison pattern: @==@ or @!=@
    constexpr auto directSelfPat = ctll::fixed_string{R"(^@\s*(==|!=)\s*@$)"};
    
    // Direct self-comparison with value pattern: @ == value
    constexpr auto selfValuePat = ctll::fixed_string{R"(^@\s*(==|!=|>=|<=|>|<)\s*(.+)$)"};

    // Try null comparisons first (more specific)
    if (auto t = parseNullCompare<dotNullPat>(s, out)) return t;
    if (auto t = parseNullCompare<brkNullPat>(s, out)) return t;
    if (auto t = parseNullCompareIndex<idxNullPat>(s, out)) return t;
    
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
                return false; // ordering comparisons with undefined are false
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
    
    // Try self comparisons
    // Direct self-comparison first (most specific)
    if (ctre::match<directSelfPat>(to_sv(s))) {
        auto m = ctre::match<directSelfPat>(to_sv(s));
        const QString op = to_qstr(m.template get<1>().to_view());
        
        Builder b{out};
        return b.add([op](const QJsonValue& j){
            // Direct self-comparison: @ == @ is always true, @ != @ is always false
            return op == "==" ? true : false;
        }, QString("@"));
    }
    
    if (auto t = parseSelfCompare<dotSelfPat>(s, out)) return t;
    if (auto t = parseSelfCompare<brkSelfPat>(s, out)) return t;
    if (auto t = parseSelfCompareIndex<idxSelfPat>(s, out)) return t;

    // Try direct self-comparison with value
    if (auto t = parseSelfValue<selfValuePat>(s, out)) return t;

    // Try regular comparisons
    if (auto t = parseCompare1<dotPat>(s, out)) return t;
    if (auto t = parseCompare1<brkPat>(s, out)) return t;
    return        parseCompareIndex<idxPat>(s, out);
}

std::optional<Token> parseRegex(QString s, QVector<FilterFn>& out)
{
    constexpr auto dotPat = ctll::fixed_string{R"(@\.([\w$]+)\s*=~\s*/(.+)/)"};
    constexpr auto brkPat = ctll::fixed_string{R"(@\[['\"]([^'"]+)['\"]\]\s*=~\s*/(.+)/)"};

    if (auto t = parseRegex1<dotPat>(s, out)) return t;
    return        parseRegex1<brkPat>(s, out);
}

std::optional<Token> parseExists(QString s, QVector<FilterFn>& out)
{
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
    constexpr auto multiSelectorPat = ctll::fixed_string{R"(^@\[([^:]+)\]$)"};
    constexpr auto negMultiSelectorPat = ctll::fixed_string{R"(^!@\[([^:]+)\]$)"};
    
    // Nested filter pattern for tests like @[?@>1] - apply filter to current array/object
    constexpr auto nestedFilterPat = ctll::fixed_string{R"(^@\[\?(.+)\]$)"};
    constexpr auto negNestedFilterPat = ctll::fixed_string{R"(^!@\[\?(.+)\]$)"};
    
    // Function to create existence test token
    auto makeExistenceToken = [&](const QString& prop) -> Token {
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

    auto makeArrayIndexToken = [&](int index)->Token {
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

    auto makeArraySliceToken = [&](int start, int end)->Token {
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

    auto makeRootToken = [&]()->Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            // RFC 9535: root existence filter checks if the root value exists
            // The root always exists unless it's explicitly undefined
            return j.type() != QJsonValue::Undefined;
        }, "@");
    };

    auto makeWildcardToken = [&]()->Token {
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

    auto makeNegatedToken = [&](const QString& prop)->Token {
        Builder b{out};
        return b.add([prop](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            // RFC 9535: negated existence filters check for property absence, not falsy values
            // A property is absent only if it's undefined (missing from the object)
            return v.type() == QJsonValue::Undefined;
        }, "!" + prop);
    };

    auto makeNegatedArrayIndexToken = [&](int index)->Token {
        Builder b{out};
        return b.add([index](const QJsonValue& j){
            if (!j.isArray()) return true; // non-arrays don't have indices
            const auto arr = j.toArray();
            if (index < 0 || index >= arr.size()) return true; // out of bounds is absent
            const auto v = arr[index];
            // RFC 9535: negated existence filters check for element absence
            return v.type() == QJsonValue::Undefined;
        }, QString("!@[%1]").arg(index));
    };

    auto makeNegatedArraySliceToken = [&](int start, int end)->Token {
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

    auto makeNegatedRootToken = [&]()->Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            // RFC 9535: negated root existence filter checks if the root value is absent
            // The root is absent only if it's explicitly undefined
            return j.type() == QJsonValue::Undefined;
        }, "!@");
    };

    auto makeNegatedWildcardToken = [&]()->Token {
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

    auto makeMultiSelectorToken = [&](const QString& selectorsStr)->Token {
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
                    bool ok = false;
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

    auto makeNegatedMultiSelectorToken = [&](const QString& selectorsStr)->Token {
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
                    bool ok = false;
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

    auto makeNestedFilterToken = [&](const QString& filterExpr)->Token {
        Builder b{out};
        return b.add([filterExpr](const QJsonValue& j){
            // Nested filter existence test: apply the filter as a JSONPath to the current value
            // For pattern like @[?@>1], we need to create a JSONPath like $[?@>1] and apply it to j
            QString jsonPathExpr = QString("$[?%1]").arg(filterExpr);
            
            // Create a temporary JSONPath to evaluate the nested filter
            if (auto path = json_query::JSONPath::create(jsonPathExpr)) {
                auto results = path->evaluateAll(j);
                // If the nested filter returns any results, the existence test passes
                return !results.empty();
            }
            return false; // Invalid filter expression or no matches
        }, QString("@[?%1]").arg(filterExpr));
    };

    auto makeNegatedNestedFilterToken = [&](const QString& filterExpr)->Token {
        Builder b{out};
        return b.add([filterExpr](const QJsonValue& j){
            // Negated nested filter existence test: apply the filter as a JSONPath and negate the result
            QString jsonPathExpr = QString("$[?%1]").arg(filterExpr);
            
            // Create a temporary JSONPath to evaluate the nested filter
            if (auto path = json_query::JSONPath::create(jsonPathExpr)) {
                auto results = path->evaluateAll(j);
                // If the nested filter returns any results, negate it (return false)
                return results.empty();
            }
            return false;
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
    if (auto m = ctre::match<negArraySlicePat>(to_sv(s))) {
        QString startStr = to_qstr(m.template get<1>().to_view());
        QString endStr = to_qstr(m.template get<2>().to_view());
        int start = startStr.isEmpty() ? 0 : startStr.toInt();
        int end = endStr.isEmpty() ? -1 : endStr.toInt();
        return makeNegatedArraySliceToken(start, end);
    }
    if (auto m = ctre::match<negRootRefPat>(to_sv(s)))
        return makeNegatedRootToken();

    // Root self-comparison pattern for $[?$==$] - compares root to itself
    constexpr auto rootSelfPat = ctll::fixed_string{R"(^\$\s*(==|!=)\s*\$$)"};
    if (ctre::match<rootSelfPat>(to_sv(s))) {
        auto m = ctre::match<rootSelfPat>(to_sv(s));
        const QString op = to_qstr(m.template get<1>().to_view());
        
        Builder b{out};
        return b.add([op](const QJsonValue& j){
            // Root self-comparison: $ == $ is always true, $ != $ is always false
            return op == "==" ? true : false;
        }, QString("$"));
    }

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

    // Nested filter patterns
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

std::optional<Token> parseSelfCmp(QString s, QVector<FilterFn>& out)
{
    static constexpr auto pat = ctll::fixed_string{R"(^@\s*(==|!=|>=|<=|>|<)\s*@$)"};
    
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

// Parse negated expressions like !(@.a=='b')
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

// Parse absolute path references like $.*.a in filter expressions
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
            // Return true if the absolute path exists (has any results)
            return !results.isEmpty();
        }
        return false;
    }, s);
}

// Forward‑declare the individual parsers ------------------------

// Table‑driven dispatch  ----------------------------------------
using RuleFn = std::optional<Token>(*)(QString, QVector<FilterFn>&);

constexpr std::array rules = {
    &parseOr,      // lowest precedence first
    &parseAnd,
    &parseNot,     // Add negation parser with high precedence
    &parseAbsolutePath, // Add absolute path parser
    &parseIn,
    &parseExists,
    &parseSelfCmp,
    &parseCompare,
    &parseRegex
};

} // namespace json_query::json_path::detail

// Table‑driven dispatch  ----------------------------------------

namespace json_query::json_path {

// Public dispatcher -----------------------------------------------------------
std::optional<Token> compileFilter(const QString& expr, QVector<FilterFn>& out)
{
    QString s = json_query::json_path::detail::stripOuterParens(expr);
    qCDebug(jsonPathLog) << "compileFilter expr=" << expr << "stripped=" << s;
    for (const auto rule : detail::rules) {
        if (auto result = rule(s, out)) {
            qCDebug(jsonPathLog) << "compileFilter accepted token kind=" << (result ? static_cast<int>(result->kind) : -1);
            return result;
        }
    }
    return std::nullopt;
}

} // namespace json_query::json_path
