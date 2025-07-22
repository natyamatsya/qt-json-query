#include "json-query/json-path/JSONPathFilter.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"

#include <QDebug>
#include "json-query/json-path/JSONPathLog.hpp"
#include <iostream>

namespace json_query::json_path::detail {

    using FilterFn = json_path::FilterFn;
    using Kind  = Token::Kind;
    using json_query::utils::to_sv;
    using json_query::utils::to_qstr;
    using json_query::json_path::detail::splitTopLevel;
    using json_query::json_path::detail::stripOuterParens;

// ───────────────────────────────────────────────────────────────
//  compileFilter  — turns [? …] into Token{Filter,…} + lambda
//      Supports three forms:
//        1.  @.prop  <op>  value          (numeric or string, == != > < >= <=)
//        2.  @['prop'] <op> value         (same operators)
//        3.  'foo' in @['arrayProp']
//      Extend with more patterns as needed.
// ───────────────────────────────────────────────────────────────
// Helper ----------------------------------------------------------------------
[[nodiscard]] inline bool unquote(QString& s)
{
    if (s.size() < 2) return false;
    const QChar quote = s.front();
    if (quote != u'"' && quote != u'\'') return false;

    // Scan forward to find the closing quote that is NOT preceded by an odd
    // number of backslashes (i.e. it is truly terminating the literal).
    int idx = -1;
    for (int i = 1; i < s.size(); ++i) {
        if (s[i] != quote) continue;
        // count preceding backslashes
        int backslashes = 0;
        int k = i - 1;
        while (k >=0 && s[k] == u'\\') { ++backslashes; --k; }
        if ((backslashes % 2) == 0) { idx = i; break; }
    }
    if (idx == -1) {
        // Fallback: assume last char is quote even if escaped; drop first and last
        if (s.back() != quote) return false;
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
// immediately obtain the corresponding Token. ------------------
struct Builder {
    QVector<FilterFn>& fns;

    [[nodiscard]] Token add(FilterFn fn, QString key = {})
    {
        fns.push_back(std::move(fn));
        const std::size_t id = fns.size() - 1;
        return Token{ Kind::Filter, 0, {}, 0u, std::move(key), id };
    }
};

// Forward‑declare the individual parsers ------------------------
[[nodiscard]] std::optional<Token> parseOr       (QString, QVector<FilterFn>&);
[[nodiscard]] std::optional<Token> parseAnd      (QString, QVector<FilterFn>&);
[[nodiscard]] std::optional<Token> parseIn       (QString, QVector<FilterFn>&);
[[nodiscard]] std::optional<Token> parseCompare  (QString, QVector<FilterFn>&);
[[nodiscard]] std::optional<Token> parseRegex    (QString, QVector<FilterFn>&);
[[nodiscard]] std::optional<Token> parseExists   (QString, QVector<FilterFn>&);
[[nodiscard]] std::optional<Token> parseSelfCmp  (QString, QVector<FilterFn>&);

// Template function forward declarations
template<auto PAT> [[nodiscard]] std::optional<Token> parseCompare1     (QString, QVector<FilterFn>&);
template<auto PAT> [[nodiscard]] std::optional<Token> parseCompareIndex (QString, QVector<FilterFn>&);
template<auto PAT> [[nodiscard]] std::optional<Token> parseNullCompare  (QString, QVector<FilterFn>&);
template<auto PAT> [[nodiscard]] std::optional<Token> parseNullCompareIndex(QString, QVector<FilterFn>&);
template<auto PAT> [[nodiscard]] std::optional<Token> parseSelfCompare  (QString, QVector<FilterFn>&);
template<auto PAT> [[nodiscard]] std::optional<Token> parseSelfCompareIndex(QString, QVector<FilterFn>&);
template<auto PAT> [[nodiscard]] std::optional<Token> parseRegex1      (QString, QVector<FilterFn>&);

// Table‑driven dispatch  ----------------------------------------
using RuleFn = std::optional<Token>(*)(QString, QVector<FilterFn>&);

constexpr std::array rules = {
    &parseOr,      // lowest precedence first
    &parseAnd,
    &parseIn,
    &parseExists,
    &parseSelfCmp,
    &parseCompare,
    &parseRegex
};

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
        R"('\s*([^']+?)\s*'\s+in\s+@\[['\"]([^'\"]+)['\"]\])"};
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

template<auto PAT>
std::optional<Token> parseCompare1(QString s, QVector<FilterFn>& out)
{
    if (auto m = ctre::match<PAT>(to_sv(s)))
    {
        const QString prop = to_qstr(m.template get<1>().to_view());
        const QString op   = to_qstr(m.template get<2>().to_view());
        QString rhs        = to_qstr(m.template get<3>().to_view()).trimmed();

        // Runtime numeric literal validator (RFC 8259 style)
        auto isValidNumberLiteral = [](QStringView v)->bool {
            if (v.isEmpty()) return false;
            int i = 0;
            if (v[i] == u'-') {
                ++i;
                if (i==v.size()) return false; // just '-'
            }
            // int part
            if (!v[i].isDigit()) return false;
            if (v[i]==u'0' && i+1< v.size() && v[i+1].isDigit()) return false; // leading zero
            while (i < v.size() && v[i].isDigit()) ++i;
            // frac part
            if (i < v.size() && v[i]==u'.') {
                ++i;
                int fracStart=i;
                while (i< v.size() && v[i].isDigit()) ++i;
                if (i==fracStart) return false; // no digits after '.'
            }
            // exponent part
            if (i < v.size() && (v[i]==u'e' || v[i]==u'E')) {
                ++i;
                if (i==v.size()) return false;
                if (v[i]==u'+' || v[i]==u'-') ++i;
                int expStart=i;
                while (i< v.size() && v[i].isDigit()) ++i;
                if (i==expStart) return false; // no digits in exponent
            }
            return i==v.size();
        };

        const bool rhsQuoted = (rhs.size() >= 2) && ((rhs.front()==u'\'' && rhs.back()==u'\'') || (rhs.front()==u'\"' && rhs.back()==u'\"'));

        bool isNum = isValidNumberLiteral(rhs);
        double num = isNum ? rhs.toDouble() : 0.0;

        const bool isBool = (!isNum && (rhs.compare("true", Qt::CaseSensitive)==0 || rhs.compare("false", Qt::CaseSensitive)==0));
        const bool boolVal = isBool ? (rhs.compare("true", Qt::CaseSensitive)==0) : false;

        // Reject unquoted RHS that is neither valid number nor boolean
        if (!isNum && !isBool && !rhsQuoted)
            return std::nullopt;

        if (!isNum && !isBool)
            (void)unquote(rhs);

        Builder b{out};
        return b.add([prop, op, isNum, num, isBool, boolVal, rhs, rhsQuoted](const QJsonValue& j) -> bool
        {
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            qCDebug(jsonPathLog).nospace() << "[flt-cmp] prop='" << prop << "' op='" << op
                                          << "' rhs=" << (isNum ? QString::number(num) : rhs)
                                          << " | v=" << v << " (type=" << v.type() << ")";
            if (isNum) {
                if (!v.isDouble()) return false;
                const double x = v.toDouble();
                qCDebug(jsonPathLog).nospace() << "  -> numeric compare lhs=" << x;
                if (op=="==") return x == num;
                if (op=="!=") return x != num;
                if (op==">")  return x >  num;
                if (op=="<")  return x <  num;
                if (op==">=") return x >= num;
                if (op=="<=") return x <= num;
                return false;
            }
            if (isBool) {
                if (!v.isBool()) return false;
                const bool b = v.toBool();
                return op=="==" ? b == boolVal
                                 : op=="!=" ? b != boolVal
                                 : false;
            }
            
            // Handle array/object deep equality comparisons
            if (rhsQuoted) {
                // Parse RHS as JSON for potential array/object comparison
                QJsonParseError parseError;
                QJsonDocument rhsDoc = QJsonDocument::fromJson(rhs.toUtf8(), &parseError);
                if (parseError.error == QJsonParseError::NoError) {
                    QJsonValue rhsValue = rhsDoc.isArray() ? QJsonValue(rhsDoc.array()) : 
                                         rhsDoc.isObject() ? QJsonValue(rhsDoc.object()) : 
                                         QJsonValue(rhs); // fallback to string
                    
                    // Deep equality comparison for arrays and objects
                    if (op == "==") return v == rhsValue;
                    if (op == "!=") return v != rhsValue;
                    // Arrays and objects don't support ordering comparisons
                    if (v.isArray() || v.isObject() || rhsValue.isArray() || rhsValue.isObject()) {
                        return false;
                    }
                }
            }
            
            // String comparisons (for quoted strings or fallback)
            const QString vs = v.toString();
            if (op=="==") return vs == rhs;
            if (op=="!=") return vs != rhs;
            if (op=="<")  return vs < rhs;
            if (op==">")  return vs > rhs;
            if (op=="<=") return vs <= rhs;
            if (op==">=") return vs >= rhs;
            return false;
        }, prop);
    }
    return std::nullopt;
}

template<auto PAT>
std::optional<Token> parseCompareIndex(QString s, QVector<FilterFn>& out)
{
    if (auto m = ctre::match<PAT>(to_sv(s)))
    {
        const int idx = to_qstr(m.template get<1>().to_view()).toInt();
        const QString op   = to_qstr(m.template get<2>().to_view());
        QString rhs        = to_qstr(m.template get<3>().to_view()).trimmed();

        // Runtime numeric literal validator (RFC 8259 style)
        auto isValidNumberLiteral = [](QStringView v)->bool {
            if (v.isEmpty()) return false;
            int i = 0;
            if (v[i] == u'-') {
                ++i;
                if (i==v.size()) return false; // just '-'
            }
            // int part
            if (!v[i].isDigit()) return false;
            if (v[i]==u'0' && i+1< v.size() && v[i+1].isDigit()) return false; // leading zero
            while (i < v.size() && v[i].isDigit()) ++i;
            // frac part
            if (i < v.size() && v[i]==u'.') {
                ++i;
                int fracStart=i;
                while (i< v.size() && v[i].isDigit()) ++i;
                if (i==fracStart) return false; // no digits after '.'
            }
            // exponent part
            if (i < v.size() && (v[i]==u'e' || v[i]==u'E')) {
                ++i;
                if (i==v.size()) return false;
                if (v[i]==u'+' || v[i]==u'-') ++i;
                int expStart=i;
                while (i< v.size() && v[i].isDigit()) ++i;
                if (i==expStart) return false; // no digits in exponent
            }
            return i==v.size();
        };

        const bool rhsQuoted = (rhs.size() >= 2) && ((rhs.front()==u'\'' && rhs.back()==u'\'') || (rhs.front()==u'\"' && rhs.back()==u'\"'));

        bool isNum = isValidNumberLiteral(rhs);
        double num = isNum ? rhs.toDouble() : 0.0;

        const bool isBool = (!isNum && (rhs.compare("true", Qt::CaseSensitive)==0 || rhs.compare("false", Qt::CaseSensitive)==0));
        const bool boolVal = isBool ? (rhs.compare("true", Qt::CaseSensitive)==0) : false;

        // Reject unquoted RHS that is neither valid number nor boolean
        if (!isNum && !isBool && !rhsQuoted)
            return std::nullopt;

        if (!isNum && !isBool)
            (void)unquote(rhs);

        Builder b{out};
        return b.add([idx, op, isNum, num, isBool, boolVal, rhs, rhsQuoted](const QJsonValue& j) -> bool
        {
            const auto arr = j.toArray();
            if (idx < 0 || idx >= arr.size()) return false;
            const auto v = arr[idx];
            qCDebug(jsonPathLog).nospace() << "[flt-cmp] idx=" << idx << " op='" << op
                                          << "' rhs=" << (isNum ? QString::number(num) : rhs)
                                          << " | v=" << v << " (type=" << v.type() << ")";
            if (isNum) {
                if (!v.isDouble()) return false;
                const double x = v.toDouble();
                qCDebug(jsonPathLog).nospace() << "  -> numeric compare lhs=" << x;
                if (op=="==") return x == num;
                if (op=="!=") return x != num;
                if (op==">")  return x >  num;
                if (op=="<")  return x <  num;
                if (op==">=") return x >= num;
                if (op=="<=") return x <= num;
                return false;
            }
            if (isBool) {
                if (!v.isBool()) return false;
                const bool b = v.toBool();
                return op=="==" ? b == boolVal
                                 : op=="!=" ? b != boolVal
                                 : false;
            }
            
            // Handle array/object deep equality comparisons
            if (rhsQuoted) {
                // Parse RHS as JSON for potential array/object comparison
                QJsonParseError parseError;
                QJsonDocument rhsDoc = QJsonDocument::fromJson(rhs.toUtf8(), &parseError);
                if (parseError.error == QJsonParseError::NoError) {
                    QJsonValue rhsValue = rhsDoc.isArray() ? QJsonValue(rhsDoc.array()) : 
                                         rhsDoc.isObject() ? QJsonValue(rhsDoc.object()) : 
                                         QJsonValue(rhs); // fallback to string
                    
                    // Deep equality comparison for arrays and objects
                    if (op == "==") return v == rhsValue;
                    if (op == "!=") return v != rhsValue;
                    // Arrays and objects don't support ordering comparisons
                    if (v.isArray() || v.isObject() || rhsValue.isArray() || rhsValue.isObject()) {
                        return false;
                    }
                }
            }
            
            // String comparisons (for quoted strings or fallback)
            const QString vs = v.toString();
            if (op=="==") return vs == rhs;
            if (op=="!=") return vs != rhs;
            if (op=="<")  return vs < rhs;
            if (op==">")  return vs > rhs;
            if (op=="<=") return vs <= rhs;
            if (op==">=") return vs >= rhs;
            return false;
        });
    }
    return std::nullopt;
}

template<auto PAT>
std::optional<Token> parseRegex1(QString s, QVector<FilterFn>& out)
{
    if (auto m = ctre::match<PAT>(to_sv(s)))
    {
        const QString prop  = to_qstr(m.template get<1>().to_view());
        const QString regex = to_qstr(m.template get<2>().to_view());
        const QRegularExpression rx(regex,
                                    QRegularExpression::CaseInsensitiveOption);

        Builder b{out};
        return b.add([prop, rx](const QJsonValue& j){
            const auto obj = j.toObject();
            return obj.value(prop).toString().contains(rx);
        }, prop);
    }
    return std::nullopt;
}

std::optional<Token> parseRegex(QString s, QVector<FilterFn>& out)
{
    constexpr auto dotPat = ctll::fixed_string{R"(@\.([\w$]+)\s*=~\s*/(.+)/)"};
    constexpr auto brkPat = ctll::fixed_string{R"(@\[['\"]([^'\"]+)['\"]\]\s*=~\s*/(.+)/)"};

    if (auto t = parseRegex1<dotPat>(s, out)) return t;
    return        parseRegex1<brkPat>(s, out);
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

    // Try null comparisons first (more specific)
    if (auto t = parseNullCompare<dotNullPat>(s, out)) return t;
    if (auto t = parseNullCompare<brkNullPat>(s, out)) return t;
    if (auto t = parseNullCompareIndex<idxNullPat>(s, out)) return t;
    
    // Try self comparisons
    if (auto t = parseSelfCompare<dotSelfPat>(s, out)) return t;
    if (auto t = parseSelfCompare<brkSelfPat>(s, out)) return t;
    if (auto t = parseSelfCompareIndex<idxSelfPat>(s, out)) return t;

    // Try regular comparisons
    if (auto t = parseCompare1<dotPat>(s, out)) return t;
    if (auto t = parseCompare1<brkPat>(s, out)) return t;
    return        parseCompareIndex<idxPat>(s, out);
}

template<auto PAT>
std::optional<Token> parseNullCompare(QString s, QVector<FilterFn>& out)
{
    if (auto m = ctre::match<PAT>(to_sv(s)))
    {
        const QString prop = to_qstr(m.template get<1>().to_view());
        const QString op   = to_qstr(m.template get<2>().to_view());

        Builder b{out};
        return b.add([prop, op](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            return op=="==" ? v.isNull() : v.type() != QJsonValue::Null;
        }, prop);
    }
    return std::nullopt;
}

template<auto PAT>
std::optional<Token> parseNullCompareIndex(QString s, QVector<FilterFn>& out)
{
    if (auto m = ctre::match<PAT>(to_sv(s)))
    {
        const int idx = to_qstr(m.template get<1>().to_view()).toInt();
        const QString op   = to_qstr(m.template get<2>().to_view());

        Builder b{out};
        return b.add([idx, op](const QJsonValue& j){
            const auto arr = j.toArray();
            if (idx < 0 || idx >= arr.size()) return false;
            const auto v = arr[idx];
            return op=="==" ? v.isNull() : v.type() != QJsonValue::Null;
        });
    }
    return std::nullopt;
}

template<auto PAT>
std::optional<Token> parseSelfCompare(QString s, QVector<FilterFn>& out)
{
    if (auto m = ctre::match<PAT>(to_sv(s)))
    {
        const QString prop = to_qstr(m.template get<1>().to_view());
        const QString op   = to_qstr(m.template get<2>().to_view());

        Builder b{out};
        return b.add([prop, op](const QJsonValue& j){
            const auto obj = j.toObject();
            const auto v = obj.value(prop);
            const auto self = j;
            return op=="==" ? v == self : v != self;
        }, prop);
    }
    return std::nullopt;
}

template<auto PAT>
std::optional<Token> parseSelfCompareIndex(QString s, QVector<FilterFn>& out)
{
    if (auto m = ctre::match<PAT>(to_sv(s)))
    {
        const int idx = to_qstr(m.template get<1>().to_view()).toInt();
        const QString op   = to_qstr(m.template get<2>().to_view());

        Builder b{out};
        return b.add([idx, op](const QJsonValue& j){
            const auto arr = j.toArray();
            if (idx < 0 || idx >= arr.size()) return false;
            const auto v = arr[idx];
            const auto self = j;
            return op=="==" ? v == self : v != self;
        });
    }
    return std::nullopt;
}

std::optional<Token> parseExists(QString s, QVector<FilterFn>& out)
{
    constexpr auto dotPat = ctll::fixed_string{R"(^@\.([\w$]+)$)"};
    constexpr auto brkPat = ctll::fixed_string{R"(^@\[['\"]([^'"]+)['\"]\]$)"};
    constexpr auto rootPat = ctll::fixed_string{R"(^@$)"};
    constexpr auto wildcardPat = ctll::fixed_string{R"(^@\.\*$)"};
    
    // Negated patterns
    constexpr auto negDotPat = ctll::fixed_string{R"(^!@\.([\w$]+)$)"};
    constexpr auto negBrkPat = ctll::fixed_string{R"(^!@\[['\"]([^'"]+)['\"]\]$)"};
    constexpr auto negRootPat = ctll::fixed_string{R"(^!@$)"};
    constexpr auto negWildcardPat = ctll::fixed_string{R"(^!@\.\*$)"};
    
    // Complex access patterns
    constexpr auto arrayIndexPat = ctll::fixed_string{R"(^@\[(\d+)\]$)"};
    constexpr auto arraySlicePat = ctll::fixed_string{R"(^@\[(\d*):(\d*)\]$)"};
    constexpr auto negArrayIndexPat = ctll::fixed_string{R"(^!@\[(\d+)\]$)"};
    constexpr auto negArraySlicePat = ctll::fixed_string{R"(^!@\[(\d*):(\d*)\]$)"};

    auto makeToken = [&](QString prop)->Token {
        Builder b{out};
        return b.add([prop](const QJsonValue& j){
            const auto v = j.toObject().value(prop);
            switch (v.type()) {
            case QJsonValue::Null:   return false;
            case QJsonValue::Bool:   return v.toBool();
            case QJsonValue::Double: return v.toDouble() != 0.0;
            case QJsonValue::String: return !v.toString().isEmpty();
            case QJsonValue::Array:  return !v.toArray().isEmpty();
            case QJsonValue::Object: return !v.toObject().isEmpty();
            default: return false;
            }
        }, prop);
    };

    auto makeArrayIndexToken = [&](int index)->Token {
        Builder b{out};
        return b.add([index](const QJsonValue& j){
            if (!j.isArray()) return false;
            const auto arr = j.toArray();
            if (index < 0 || index >= arr.size()) return false;
            const auto& v = arr[index];
            switch (v.type()) {
            case QJsonValue::Null:   return false;
            case QJsonValue::Bool:   return v.toBool();
            case QJsonValue::Double: return v.toDouble() != 0.0;
            case QJsonValue::String: return !v.toString().isEmpty();
            case QJsonValue::Array:  return !v.toArray().isEmpty();
            case QJsonValue::Object: return !v.toObject().isEmpty();
            default: return false;
            }
        }, QString("@[%1]").arg(index));
    };

    auto makeArraySliceToken = [&](int start, int end)->Token {
        Builder b{out};
        return b.add([start, end](const QJsonValue& j){
            if (!j.isArray()) return false;
            const auto arr = j.toArray();
            int actualStart = start < 0 ? 0 : start;
            int actualEnd = end < 0 ? arr.size() : qMin(end, arr.size());
            // Check if slice has any truthy elements
            for (int i = actualStart; i < actualEnd; ++i) {
                const auto& v = arr[i];
                switch (v.type()) {
                case QJsonValue::Null:   continue;
                case QJsonValue::Bool:   if (v.toBool()) return true; break;
                case QJsonValue::Double: if (v.toDouble() != 0.0) return true; break;
                case QJsonValue::String: if (!v.toString().isEmpty()) return true; break;
                case QJsonValue::Array:  if (!v.toArray().isEmpty()) return true; break;
                case QJsonValue::Object: if (!v.toObject().isEmpty()) return true; break;
                default: continue;
                }
            }
            return false;
        }, QString("@[%1:%2]").arg(start).arg(end));
    };

    auto makeRootToken = [&]()->Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            switch (j.type()) {
            case QJsonValue::Null:   return false;
            case QJsonValue::Bool:   return j.toBool();
            case QJsonValue::Double: return j.toDouble() != 0.0;
            case QJsonValue::String: return !j.toString().isEmpty();
            case QJsonValue::Array:  return !j.toArray().isEmpty();
            case QJsonValue::Object: return !j.toObject().isEmpty();
            default: return false;
            }
        }, "@");
    };

    auto makeNegatedToken = [&](QString prop)->Token {
        Builder b{out};
        return b.add([prop](const QJsonValue& j){
            const auto v = j.toObject().value(prop);
            switch (v.type()) {
            case QJsonValue::Null:   return true;  // negated: null is falsy, so !null is true
            case QJsonValue::Bool:   return !v.toBool();
            case QJsonValue::Double: return v.toDouble() == 0.0;
            case QJsonValue::String: return v.toString().isEmpty();
            case QJsonValue::Array:  return v.toArray().isEmpty();
            case QJsonValue::Object: return v.toObject().isEmpty();
            default: return true; // undefined is falsy, so !undefined is true
            }
        }, "!" + prop);
    };

    auto makeNegatedArrayIndexToken = [&](int index)->Token {
        Builder b{out};
        return b.add([index](const QJsonValue& j){
            if (!j.isArray()) return true; // non-arrays don't have indices
            const auto arr = j.toArray();
            if (index < 0 || index >= arr.size()) return true; // out of bounds is falsy
            const auto& v = arr[index];
            switch (v.type()) {
            case QJsonValue::Null:   return true;  // negated: null is falsy
            case QJsonValue::Bool:   return !v.toBool();
            case QJsonValue::Double: return v.toDouble() == 0.0;
            case QJsonValue::String: return v.toString().isEmpty();
            case QJsonValue::Array:  return v.toArray().isEmpty();
            case QJsonValue::Object: return v.toObject().isEmpty();
            default: return true; // undefined is falsy
            }
        }, QString("!@[%1]").arg(index));
    };

    auto makeNegatedArraySliceToken = [&](int start, int end)->Token {
        Builder b{out};
        return b.add([start, end](const QJsonValue& j){
            if (!j.isArray()) return true; // non-arrays don't have slices
            const auto arr = j.toArray();
            int actualStart = start < 0 ? 0 : start;
            int actualEnd = end < 0 ? arr.size() : qMin(end, arr.size());
            // Check if slice has NO truthy elements (negated)
            for (int i = actualStart; i < actualEnd; ++i) {
                const auto& v = arr[i];
                switch (v.type()) {
                case QJsonValue::Null:   continue; // null is falsy, doesn't affect result
                case QJsonValue::Bool:   if (v.toBool()) return false; break; // found truthy, so negated is false
                case QJsonValue::Double: if (v.toDouble() != 0.0) return false; break;
                case QJsonValue::String: if (!v.toString().isEmpty()) return false; break;
                case QJsonValue::Array:  if (!v.toArray().isEmpty()) return false; break;
                case QJsonValue::Object: if (!v.toObject().isEmpty()) return false; break;
                default: continue;
                }
            }
            return true; // No truthy elements found, so negated is true
        }, QString("!@[%1:%2]").arg(start).arg(end));
    };

    auto makeNegatedRootToken = [&]()->Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            switch (j.type()) {
            case QJsonValue::Null:   return true;  // negated: null is falsy, so !null is true
            case QJsonValue::Bool:   return !j.toBool();
            case QJsonValue::Double: return j.toDouble() == 0.0;
            case QJsonValue::String: return j.toString().isEmpty();
            case QJsonValue::Array:  return j.toArray().isEmpty();
            case QJsonValue::Object: return j.toObject().isEmpty();
            default: return true; // undefined is falsy, so !undefined is true
            }
        }, "!@");
    };

    auto makeWildcardToken = [&]()->Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            if (!j.isObject()) return false;
            const auto obj = j.toObject();
            // Check if any property exists and is truthy
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                const auto& v = it.value();
                switch (v.type()) {
                case QJsonValue::Null:   continue; // null is falsy
                case QJsonValue::Bool:   if (v.toBool()) return true; break;
                case QJsonValue::Double: if (v.toDouble() != 0.0) return true; break;
                case QJsonValue::String: if (!v.toString().isEmpty()) return true; break;
                case QJsonValue::Array:  if (!v.toArray().isEmpty()) return true; break;
                case QJsonValue::Object: if (!v.toObject().isEmpty()) return true; break;
                default: continue;
                }
            }
            return false; // No truthy properties found
        }, "@.*");
    };

    auto makeNegatedWildcardToken = [&]()->Token {
        Builder b{out};
        return b.add([](const QJsonValue& j){
            if (!j.isObject()) return true; // non-objects don't have properties, so !@.* is true
            const auto obj = j.toObject();
            // Check if NO property exists and is truthy (negated wildcard)
            for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
                const auto& v = it.value();
                switch (v.type()) {
                case QJsonValue::Null:   continue; // null is falsy, doesn't affect result
                case QJsonValue::Bool:   if (v.toBool()) return false; break; // found truthy, so !@.* is false
                case QJsonValue::Double: if (v.toDouble() != 0.0) return false; break;
                case QJsonValue::String: if (!v.toString().isEmpty()) return false; break;
                case QJsonValue::Array:  if (!v.toArray().isEmpty()) return false; break;
                case QJsonValue::Object: if (!v.toObject().isEmpty()) return false; break;
                default: continue;
                }
            }
            return true; // No truthy properties found, so !@.* is true
        }, "!@.*");
    };

    // Check negated patterns first (more specific)
    if (auto m = ctre::match<negRootPat>(to_sv(s)))
        return makeNegatedRootToken();
    if (auto m = ctre::match<negWildcardPat>(to_sv(s)))
        return makeNegatedWildcardToken();
    if (auto m = ctre::match<negArrayIndexPat>(to_sv(s))) {
        int index = to_qstr(m.template get<1>().to_view()).toInt();
        return makeNegatedArrayIndexToken(index);
    }
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

    // Check for root existence filter
    if (auto m = ctre::match<rootPat>(to_sv(s)))
        return makeRootToken();
    
    // Check for wildcard existence filter
    if (auto m = ctre::match<wildcardPat>(to_sv(s)))
        return makeWildcardToken();
    
    // Check for array access patterns
    if (auto m = ctre::match<arrayIndexPat>(to_sv(s))) {
        int index = to_qstr(m.template get<1>().to_view()).toInt();
        return makeArrayIndexToken(index);
    }
    if (auto m = ctre::match<arraySlicePat>(to_sv(s))) {
        QString startStr = to_qstr(m.template get<1>().to_view());
        QString endStr = to_qstr(m.template get<2>().to_view());
        int start = startStr.isEmpty() ? 0 : startStr.toInt();
        int end = endStr.isEmpty() ? -1 : endStr.toInt();
        return makeArraySliceToken(start, end);
    }
    
    if (auto m = ctre::match<dotPat>(to_sv(s)))
        return makeToken(to_qstr(m.template get<1>().to_view()));
    if (auto m = ctre::match<brkPat>(to_sv(s)))
        return makeToken(to_qstr(m.template get<1>().to_view()));
    return std::nullopt;
}

std::optional<Token> parseSelfCmp(QString s, QVector<FilterFn>& out)
{
    static constexpr auto pat = ctll::fixed_string{R"(^@\s*(==|!=|>=|<=|>|<)\s*(.+)$)"};
    if (auto m = ctre::match<pat>(to_sv(s)))
    {
        const QString op  = to_qstr(m.template get<1>().to_view());
        QString rhs       = to_qstr(m.template get<2>().to_view()).trimmed();

        bool   isNum = false;
        double num   = rhs.toDouble(&isNum);
        const bool rhsQuoted = (rhs.startsWith('\'') && rhs.endsWith('\'')) || (rhs.startsWith('"') && rhs.endsWith('"'));
        const bool isBool  = (rhs.compare("true", Qt::CaseSensitive)==0 || rhs.compare("false", Qt::CaseSensitive)==0);
        bool boolVal = false;
        if (isBool) boolVal = (rhs.compare("true", Qt::CaseSensitive)==0);

        if (!isNum && !isBool && !rhsQuoted)
            return std::nullopt;

        if (!isNum && !isBool)
            (void)unquote(rhs);

        auto cmpScalar=[op,isNum,num,isBool,boolVal,rhs](const QJsonValue& v)->bool{
            if (isNum) {
                if (!v.isDouble()) return false;
                const double x=v.toDouble();
                if (op=="==") return x==num;
                if (op=="!=") return x!=num;
                if (op==">") return x>num;
                if (op=="<") return x<num;
                if (op==">=") return x>=num;
                if (op=="<=") return x<=num;
                return false;
            }
            if (isBool){
                if(!v.isBool()) return false;
                bool b=v.toBool();
                return op=="=="?b==boolVal: op=="!="?b!=boolVal:false;
            }
            QString s=v.toString();
            return op=="=="?s==rhs: op=="!="?s!=rhs:false;
        };

        Builder b{out};
        return b.add([cmpScalar](const QJsonValue& j){
            std::function<bool(const QJsonValue&)> visit = [&](const QJsonValue& v)->bool {
                switch (v.type()) {
                case QJsonValue::Array:
                    for (const auto& elem : v.toArray())
                        if (visit(elem)) return true;
                    return false;
                case QJsonValue::Object: {
                    const auto obj = v.toObject();
                    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it)
                        if (visit(it.value())) return true;
                    return false;
                }
                default:
                    return cmpScalar(v);
                }
            };
            return visit(j);
        });
    }
    return std::nullopt;
}

} // namespace json_query::json_path::detail

// ──────────────────────────────────────────────────────────────────────────────

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
