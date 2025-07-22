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

    // Unescape backslash sequences
    s.replace("\\\\", "\\");   // \\ -> \
    s.replace("\\'", "'");       // \\' -> '
    s.replace("\\\"", "\"");   // \\" -> "
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

// Table‑driven dispatch  ----------------------------------------
using RuleFn = std::optional<Token>(*)(QString, QVector<FilterFn>&);

constexpr std::array rules = {
    &parseOr,      // lowest precedence first
    &parseAnd,
    &parseIn,
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

        const bool isBool = (!isNum && (rhs == "true" || rhs == "false"));
        const bool boolVal = isBool ? (rhs == "true") : false;

        // Reject unquoted RHS that is neither valid number nor boolean
        if (!isNum && !isBool && !rhsQuoted)
            return std::nullopt;

        if (!isNum && !isBool)
            (void)unquote(rhs);

        Builder b{out};
        return b.add([prop, op, isNum, num, isBool, boolVal, rhs](const QJsonValue& j) -> bool
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
            const QString vs = v.toString();
            return op=="==" ? vs == rhs
                 : op=="!=" ? vs != rhs
                 : false;
        }, prop);
    }
    return std::nullopt;
}

std::optional<Token> parseCompare(QString s, QVector<FilterFn>& out)
{
    constexpr auto dotPat = ctll::fixed_string{R"(@\.([\w$]+)\s*(==|!=|>=|<=|>|<)\s*(.+))"};
    constexpr auto brkPat = ctll::fixed_string{R"(@\[['\"]([^'\"]+)['\"]\]\s*(==|!=|>=|<=|>|<)\s*(.+))"};

    if (auto t = parseCompare1<dotPat>(s, out)) return t;
    return        parseCompare1<brkPat>(s, out);
}

// -----------------------------------------------------------------------------
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
