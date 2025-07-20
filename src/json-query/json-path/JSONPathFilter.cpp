#include "json-query/json-path/JSONPathFilter.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"

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
    if (s.size() >= 2 &&
        ((s.front()==u'"' && s.back()==u'"') ||
         (s.front()==u'\''&& s.back()==u'\'')))
    {
        s.remove(0,1);          // drop leading
        s.chop(1);                      // drop trailing
        return true;
    }
    return false;
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

        bool isNum = false;
        const double num = rhs.toDouble(&isNum);
        const bool isBool = (!isNum && (rhs == "true" || rhs == "false"));
        const bool boolVal = isBool ? (rhs == "true") : false;
        if (!isNum && !isBool) (void)unquote(rhs);

        Builder b{out};
        return b.add([prop, op, isNum, num, isBool, boolVal, rhs](const QJsonValue& j) -> bool
        {
            const auto obj = j.toObject();
            qDebug() << "[flt] obj keys" << obj.keys();
            const auto v = obj.value(prop);
            qDebug() << "[flt] compare prop" << prop << "val=" << v << "isNum" << isNum;
            if (isNum) {
                if (!v.isDouble()) return false;
                const double x = v.toDouble();
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
    for (const auto rule : detail::rules) {
        if (auto result = rule(s, out))
            return result;
    }
    return std::nullopt;
}

} // namespace json_query::json_path
