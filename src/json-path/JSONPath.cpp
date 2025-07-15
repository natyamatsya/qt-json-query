// jsonpath.cpp - Using CTRE
#include "json-query/JSONPath.hpp"
#include <vector>
#include <regex>
#include <deque>

#include <QRegularExpression>

#include "json-query/JSONQueryUtils.hpp"
#include "json-query/ContainerCursor.hpp"
#include "json-query/QtHash.hpp"

using json_query::ContainerCursor;
using json_query::Slice;
using json_query::Token;
using json_query::Error;

// ──────────────────────────────────────────────────────────────────────
//  Helpers local to this TU
// ──────────────────────────────────────────────────────────────────────
namespace {

    // Parse [start:end:step]   (empty parts get defaults)
    [[nodiscard]] static inline Slice makeSlice(QStringView v)
    {
        auto readInt = [](QStringView part, int def) {
            if (part.isEmpty()) return def;
            bool ok = false;  int x = part.toInt(&ok);
            return ok ? x : def;
        };

        const qsizetype c1 = v.indexOf(u':');
        const qsizetype c2 = v.indexOf(u':', c1 + 1);

        const int start = readInt(v.sliced(1, c1 - 1),                  0);
        const int end   = readInt(c2 == -1 ? v.sliced(c1 + 1, v.size()-c1-2)
                                           : v.sliced(c1 + 1, c2 - c1 - 1),
                                  std::numeric_limits<int>::max());
        const int step  = c2 == -1 ? 1
                                   : readInt(v.sliced(c2 + 1, v.size()-c2-2), 1);
        return { start, end, step };
    }

    //  src/json-path/JSONPath.cpp
    static inline QString stripOuterParens(QStringView sv)
    {
        // remove exactly one balanced pair if present
        if (sv.size() >= 2 && sv.front() == u'(' && sv.back() == u')')
            sv = sv.mid(1, sv.size() - 2);

        return QString(sv).trimmed();   // QString ctor understands QStringView
    }

  // ─────────────────────────────────────────────────────────────────────────────
// Helper: split at the first occurrence of delim that is **not** in parentheses
// Returns { left , right }  if found,  otherwise std::nullopt
// ─────────────────────────────────────────────────────────────────────────────
auto splitTopLevel = [](QStringView sv, QLatin1StringView delim)
        -> std::optional<std::pair<QString,QString>>
{
    const qsizetype nDelim = delim.size();
    int parenDepth = 0;

    for (qsizetype i = 0, N = sv.size() - nDelim + 1; i < N; ++i) {
        const QChar c = sv[i];
        if (c == u'(')         ++parenDepth;
        else if (c == u')')    --parenDepth;
        else if (parenDepth == 0 && sv.mid(i, nDelim) == delim) {
            return std::pair<QString,QString>{
                QString(sv.left(i)),
                QString(sv.mid(i + nDelim)) };
        }
    }
    return std::nullopt;
};

} // namespace

JSONPath::Result JSONPath::create(QStringView rawPath, Option opt)
{
    QString path = rawPath.toString();
    // Extract any trailing function → updates `path` and yields `func`
    FunctionType func = detectTrailingFunction(path);

    if (path.isEmpty())
        return std::unexpected(Error::EmptySegment);      // choose your enum

    // Compile into tokens + filter list
    auto compiled = compilePath(path);       // the expected<> helper
    if (!compiled)                               // → error bubbled up
        return std::unexpected(compiled.error());

    // Success: build the object
    return JSONPath(opt,
                    func,
                    rawPath.toString(),          // keep original as given
                    std::move(compiled->tokens), // tokens created in impl
                    std::move(compiled->filters));
}

JSONPath::FunctionType JSONPath::detectTrailingFunction(QString& path)
{
    using enum FunctionType;

    static const QPair<QString, FunctionType> table[] = {
        {".length()", Length},
        {".min()",    Min   },
        {".max()",    Max   },
    };

    for (auto& p : table)
        if (path.endsWith(p.first))
        {
            path.chop(p.first.size());
            return p.second;
        }

    return None;
}

namespace detail {

// ────────────────────────────────────────────────────────────────
// 1. pushKey helper (renamed)
// ────────────────────────────────────────────────────────────────
struct KeyBuilder {
    QVector<Token>& tgt;

    std::expected<void,Error> push(QStringView key)
    {
        if (key.isEmpty()) return std::unexpected(Error::EmptySegment);
        QString s = key.toString();
        tgt.append(Token{ Token::Kind::Key, 0, {}, qt_hash(s), std::move(s), 0 });
        return {};
    }
};

// ────────────────────────────────────────────────────────────────
// 2. dot‑segment parser (no switch on QChar)
// ────────────────────────────────────────────────────────────────
[[nodiscard]] std::expected<qsizetype,Error>
parseDot(qsizetype pos, QStringView sv,
         KeyBuilder& kb, QVector<Token>& tokens)
{
    const qsizetype n = sv.size();
    if (++pos >= n) return std::unexpected(Error::TrailingDot);

    QChar nxt = sv[pos];
    if (nxt == u'.') { tokens.append(Token{Token::Kind::Recursive}); ++pos; return pos; }
    if (nxt == u'*') { tokens.append(Token{Token::Kind::Wildcard }); ++pos; return pos; }

    qsizetype start = pos;
    while (pos < n && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
    if (auto r = kb.push(sv.sliced(start, pos - start)); !r)
        return std::unexpected(r.error());
    return pos;
}

// Friend‑level wrapper so rule lambdas can call compileFilter  ----------------
inline std::optional<Token>
callCompileFilter(const QString& expr, QVector<JSONPath::FilterFn>& f)
{
    return JSONPath::compileFilter(expr, f);        // still private
}

// A small façade so every rule can emit tokens consistently
    // BracketSink now carries *filters* as well
    struct BracketSink {
    QVector<Token>&   tk;
    KeyBuilder&       kb;
    QVector<JSONPath::FilterFn>& filters;

    std::expected<void,Error> key(QStringView k) { return kb.push(k); }
    void wild()                 { tk.append(Token{Token::Kind::Wildcard}); }
    void slice(const Slice& s)   { tk.append(Token{Token::Kind::Slice,0,s,0u}); }
    void index(int i)            { tk.append(Token{Token::Kind::Index,i}); }
    void pushFilter(const Token& t){ tk.append(t); }
};


// A helper to iterate over the content and run rules.
using BrRule = std::function<std::optional<Error>(QStringView, BracketSink&)>;

static const std::array<BrRule,6> BR_RULES = {{

    // 1.  *      ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (content == u"*") { out.wild(); return {}; }
        return std::nullopt;
    },

    // 2.  ?(filter)  -----------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.isEmpty() && content.front()==u'?') {
            auto tk = callCompileFilter(content.mid(1).toString(), out.filters);
            if (!tk) return Error::UnsupportedFilter;
            out.pushFilter(*tk);
            return {};
        }
        return std::nullopt;
    },

    // 3.  slice      -----------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (content.contains(u':')) {
            out.slice(makeSlice(content));
            return {};
        }
        return std::nullopt;
    },

    // 4.  quoted key -----------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.isEmpty() &&
           (content.front()==u'\'' || content.front()==u'\"'))
        {
            const qsizetype qEnd = content.lastIndexOf(content.front());
            if (qEnd <= 0) return Error::UnmatchedQuote;
            if (auto r = out.key(content.sliced(1,qEnd-1)); !r)
                return r.error();
            return {};
        }
        return std::nullopt;
    },

    // 5.  numeric index --------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        bool ok=false; int idx = QString(content).toInt(&ok);
        if (ok) { out.index(idx); return {}; }
        return std::nullopt;
    },

    // 6.  fallback raw key -----------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        const QString wrapped = "[" + QString(content) + "]";
        if (auto r = out.key(wrapped); !r) return r.error();
        return {};
    }
}};

// ------------------------------------------------------------------
// parseBracket dispatcher
// ------------------------------------------------------------------
[[nodiscard]] std::expected<qsizetype, Error>
parseBracket(qsizetype pos,
             QStringView          sv,
             KeyBuilder&          kb,
             QVector<Token>&      tokens,
             QVector<JSONPath::FilterFn>&   filters)
    {
        using K = Token::Kind;

        // 1. find the matching ’]’ -------------------------------------------------
        const qsizetype n      = sv.size();
        const qsizetype startB = ++pos;                 // skip initial '['

        int depth = 1;
        while (pos < n && depth) {
            if      (sv[pos] == u'[') ++depth;
            else if (sv[pos] == u']') --depth;
            ++pos;
        }
        if (depth) return std::unexpected(Error::UnmatchedBracket);

        const QStringView content = sv.sliced(startB, pos - startB - 1);

        // 2. run rule table until one rule adds something --------------------------
        BracketSink sink{tokens, kb, filters};

        for (auto&& rule : BR_RULES)
        {
            const std::size_t tokBefore   = tokens .size();
            const std::size_t filtBefore  = filters.size();

            if (auto err = rule(content, sink))               // matched but errored
                return std::unexpected(*err);

            // did this rule append at least one token or filter?
            if (tokens.size() != tokBefore || filters.size() != filtBefore)
                return pos;                                   // handled → stop
        }

        // Fallback rule in BR_RULES always handles the segment, so we should never
        // reach this line. If we do, return a generic syntax error.
        return std::unexpected(Error::InvalidIndex);
    }

// bare‑name parser: replace kb.pushKey → kb.push
[[nodiscard]] std::expected<qsizetype,Error>
parseBare(qsizetype pos, QStringView sv, KeyBuilder& kb)
{
    qsizetype n = sv.size(), start = pos;
    while (pos < n && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
    if (auto r = kb.push(sv.sliced(start, pos - start)); !r)
        return std::unexpected(r.error());
    return pos;
}
} // namespace detail

std::expected<JSONPath::Compiled, Error>
JSONPath::compilePath(QStringView sv)
{
    using K = Token::Kind;
    QVector<Token> tokens;
    QVector<FilterFn> filters;
    tokens.reserve(sv.count('.') + sv.count('[') + 4);

    // root
    if (sv.empty() || (sv[0] != u'$' && sv[0] != u'@'))
        return std::unexpected(Error::MissingRoot);
    tokens.append(Token{ K::Key, 0, {}, qt_hash(sv.first(1)),
                         sv.first(1).toString(), 0 });

    detail::KeyBuilder kb{tokens};

    // scan
    for (qsizetype pos = 1, n = sv.size(); pos < n; )
    {
        std::expected<qsizetype,Error> next =
            (sv[pos] == u'.') ? detail::parseDot    (pos, sv, kb, tokens)
          : (sv[pos] == u'[') ? detail::parseBracket(pos, sv, kb, tokens, filters)
          :                    detail::parseBare   (pos, sv, kb);

        if (!next) return std::unexpected(next.error());
        pos = *next;
    }
    return Compiled{ std::move(tokens), std::move(filters) };
}

QJsonValue JSONPath::evaluate(const QJsonDocument &document) const
{
    const QJsonValue root = document.isArray() ? QJsonValue(document.array())
                                               : QJsonValue(document.object());
    return evaluate(root);
}

// ─────────────────────────────────────────────────────────────────────
//  evaluate (token pipeline)
// ─────────────────────────────────────────────────────────────────────
QJsonValue JSONPath::evaluate(const QJsonValue& root) const
{
    if (m_option == Option::AsPathList)
        return evalAsPathList(*this, root);

    return evalStandard(*this, root);
}

// ─────────────────────────────────────────────────────────────────────
//  evaluateToken – apply a single Token to one QJsonValue
//      * Returns an array with all matches (empty ⇒ no match).
//      * Fast‑path exits keep nesting shallow.
// ─────────────────────────────────────────────────────────────────────
// -----------------------------------------------------------------------------
// Inline evaluators — one per token kind
// -----------------------------------------------------------------------------
namespace detail {

template<JSONPath::Token::Kind K>
inline QJsonArray eval(const JSONPath& jp,
                       const JSONPath::Token& tk,
                       const QJsonValue& v);

// --- Key -------------------------------------------------------------------
template<>
inline QJsonArray eval<JSONPath::Token::Kind::Key>(const JSONPath&,
                                                   const JSONPath::Token& tk,
                                                   const QJsonValue& v)
{
    QJsonArray out;
    if (!v.isObject()) return out;

    const auto obj = v.toObject();
    if (auto it = obj.find(tk.key); it != obj.end())
        out.append(it.value());
    return out;
}

// --- Index -----------------------------------------------------------------
template<>
inline QJsonArray eval<JSONPath::Token::Kind::Index>(const JSONPath& jp,
                                                     const JSONPath::Token& tk,
                                                     const QJsonValue& v)
{
    QJsonArray out;
    if (!v.isArray()) return out;

    const auto& arr = v.toArray();
    const int idx   = jp.normalizeIndex(static_cast<int>(tk.index), arr.size());
    if (idx >= 0 && idx < arr.size()) out.append(arr[idx]);
    return out;
}

// --- Slice -----------------------------------------------------------------
template<>
inline QJsonArray eval<JSONPath::Token::Kind::Slice>(const JSONPath& jp,
                                                     const JSONPath::Token& tk,
                                                     const QJsonValue& v)
{
    if (v.isArray())
        return jp.evalSlice(v.toArray(), tk.slice);
    return {};
}

// --- Wildcard --------------------------------------------------------------
template<>
inline QJsonArray eval<JSONPath::Token::Kind::Wildcard>(const JSONPath& jp,
                                                        const JSONPath::Token&,
                                                        const QJsonValue& v)
{
    if (v.isObject()) return jp.wildcardObject(v.toObject());
    if (v.isArray())  return jp.wildcardArray (v.toArray());
    return {};
}

// --- Recursive -------------------------------------------------------------
template<>
inline QJsonArray eval<JSONPath::Token::Kind::Recursive>(const JSONPath& jp,
                                                         const JSONPath::Token&,
                                                         const QJsonValue& v)
{
    return jp.evaluateRecursive(v, 0);
}

// --- Filter ----------------------------------------------------------------
template<>
inline QJsonArray eval<JSONPath::Token::Kind::Filter>(const JSONPath& jp,
                                                      const JSONPath::Token& tk,
                                                      const QJsonValue& v)
{
    QJsonArray out;
    if (tk.filterId >= jp.m_filters.size()) return out;

    const auto& pred = jp.m_filters[tk.filterId];
    if (v.isArray()) {
        for (const auto& e : v.toArray())
            if (pred(e)) out.append(e);
        return out;
    }
    if (pred(v)) out.append(v);
    return out;
}

} // namespace detail

// -----------------------------------------------------------------------------
// Fast dispatch table: Kind → evaluator
// -----------------------------------------------------------------------------
inline QJsonArray JSONPath::evaluateToken(const Token& tk,
                                          const QJsonValue& v) const
{
    using namespace detail;
    using enum Token::Kind;
    // NB: keep the order in sync with the enum!
    constexpr std::array<
        QJsonArray (*)(const JSONPath&, const Token&, const QJsonValue&), 6> lut = {
        eval<Key>,
        eval<Index>,
        eval<Slice>,
        eval<Wildcard>,
        eval<Recursive>,
        eval<Filter>
    };

    return lut[static_cast<std::size_t>(tk.kind)](*this, tk, v);
}


QJsonArray JSONPath::evaluateAll(const QJsonDocument &document) const
{
    return evaluateAll(document.isArray() ? QJsonValue(document.array())
                                          : QJsonValue(document.object()));
}

QJsonArray JSONPath::evaluateAll(const QJsonValue &value) const
{
    QJsonValue res = evaluate(value);
    if (res.isArray())
        return res.toArray();
    if (res.isUndefined() || res.isNull())
        return {};
    return QJsonArray{res};
}

namespace detail {

    using ::json_query::FilterFn;
    using Token = json_query::Token;
    using Kind  = Token::Kind;
    using json_query::utils::to_sv;
    using json_query::utils::to_qstr;

// ───────────────────────────────────────────────────────────────
//  compileFilter  — turns [? …] into Token{Filter,…} + lambda
//      Supports three forms:
//        1.  @.prop  <op>  value          (numeric or string, == != > < >= <=)
//        2.  @['prop'] <op> value         (same operators)
//        3.  'foo' in @['arrayProp']
//      Extend with more patterns as needed.
// ───────────────────────────────────────────────────────────────
// === replace the very top of JSONPath::compileFilter =========================
// Helper ----------------------------------------------------------------------
[[nodiscard]] inline bool unquote(QString& s)
{
    if (s.size() >= 2 &&
        ((s.front()==u'"' && s.back()==u'"') ||
         (s.front()==u'\''&& s.back()==u'\'')))
    {
        s.remove(0,1);                  // drop leading
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

// ────────────────────────── parser implementations ───────────────────────────
std::optional<Token> parseOr(QString s, QVector<FilterFn>& out)
{
    if (auto split = splitTopLevel(s, "||"_L1); split)
    {
        const auto& [lhsS, rhsS] = *split;

        QVector<FilterFn> tmp;
        auto lhsT = JSONPath::compileFilter(lhsS.trimmed(), tmp);
        auto rhsT = JSONPath::compileFilter(rhsS.trimmed(), tmp);
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
        auto lhsT = JSONPath::compileFilter(lhsS.trimmed(), tmp);
        auto rhsT = JSONPath::compileFilter(rhsS.trimmed(), tmp);
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
        if (!isNum) (void)unquote(rhs);

        Builder b{out};
        return b.add([prop, op, isNum, num, rhs](const QJsonValue& j) -> bool
        {
            const auto v = j[prop];
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
            return j[prop].toString().contains(rx);
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

} // unnamed namespace
// ──────────────────────────────────────────────────────────────────────────────

// Public dispatcher -----------------------------------------------------------
std::optional<Token>
JSONPath::compileFilter(const QString& expr, QVector<FilterFn>& out)
{
    QString s = stripOuterParens(expr);
    for (detail::RuleFn fn : detail::rules)
        if (auto t = fn(s, out))  return t;
    return std::nullopt;          // unsupported
}

// ===================================================================
//  Wild‑cards  (object / array) – used by the * token
// ===================================================================
QJsonArray JSONPath::wildcardObject(const QJsonObject& obj) const
{
    QJsonArray out;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        out.append(it.value());
    return out;
}

QJsonArray JSONPath::wildcardArray(const QJsonArray& arr) const
{
    return arr;                      // shallow copy; Qt is implicit‑shared
}

// ===================================================================
//  Recursive‑descent – collect *all* descendant containers
// ===================================================================
QJsonArray JSONPath::evaluateRecursive(const QJsonValue& value,
                                       int /*unused*/) const
{
    QJsonArray out;
    if (!value.isArray() && !value.isObject())
        return out;

    std::deque<QJsonValue> queue;
    queue.push_back(value);
    while (!queue.empty())
    {
        QJsonValue cur = queue.front();
        queue.pop_front();
        // store the container itself
        out.append(cur);

        if (cur.isObject()) {
            const QJsonObject obj = cur.toObject();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                const QJsonValue& child = it.value();
                if (child.isArray() || child.isObject())
                    queue.push_back(child);
            }
        } else {                     // array
            const QJsonArray arr = cur.toArray();
            for (const auto& child : arr)
                if (child.isArray() || child.isObject())
                    queue.push_back(child);
        }
    }
    return out;
}

// ===================================================================
//  Slice helper + negative‑index helper  (already declared in header)
// ===================================================================
QJsonArray JSONPath::evalSlice(const QJsonArray& array,
                               const Slice& s) const
{
    QJsonArray out;
    if (s.step <= 0) return out;

    const int size = array.size();
    auto norm = [size](int i) { return i < 0 ? size + i : i; };

    int begin = norm(static_cast<int>(s.start));
    int end   = (s.end == std::numeric_limits<qsizetype>::max())
                    ? size
                    : norm(static_cast<int>(s.end));

    for (int i = begin; i < end && i < size; i += static_cast<int>(s.step))
        if (i >= 0) out.append(array[i]);
    return out;
}

int JSONPath::normalizeIndex(int idx, int size) const
{
    return idx < 0 ? size + idx : idx;
}

// ───────────────────────────────────────────────────────────────
//  segmentToPointer – JSONPath piece  →  RFC‑6901 JSON‑Pointer
//      * Handles dot‑ and bracket‑notation
//      * No heap work except final QString
// ───────────────────────────────────────────────────────────────
QString JSONPath::segmentToPointer(const QString& seg) const
{
    // trivial cases
    if (seg.isEmpty() || seg == "$" || seg == "@")
        return QString();

    QStringView sv{seg};
    QString out;  out.reserve(seg.size()*2);  // worst case “~”→“~0”, “/”→“~1”

    // helper: append and escape
    auto push = [&](QStringView piece)
    {
        out += u'/';
        for (const QChar ch : piece)
        {
            if (ch == u'~')      out += QLatin1String("~0");
            else if (ch == u'/') out += QLatin1String("~1");
            else                 out += ch;
        }
    };

    qsizetype pos = 0, n = sv.size();
    if (sv[0] == u'$' || sv[0] == u'@') ++pos;          // skip root

    while (pos < n)
    {
        if (sv[pos] == u'.') { ++pos; continue; }

        if (sv[pos] == u'[') {                          // [ ... ]
            ++pos;
            bool quoted = (sv[pos] == u'\'' || sv[pos] == u'\"');
            if (quoted) ++pos;
            qsizetype start = pos;
            while (pos < n && (quoted ? sv[pos]!=sv[start-1] : sv[pos]!=u']'))
                ++pos;
            push(sv.sliced(start, pos-start));
            while (pos < n && sv[pos] != u']') ++pos;
            ++pos;                                     // skip ']'
        }
        else {                                         // plain id
            qsizetype start = pos;
            while (pos < n && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
            push(sv.sliced(start, pos-start));
        }
    }
    return out;
}

QJsonValue JSONPath::evalAsPathList(const JSONPath& self,
                                    const QJsonValue& root)
{
    Q_UNUSED(root)
    if (self.m_option != Option::AsPathList)
        return QJsonValue(QJsonValue::Undefined);

    QStringList segments;
    for (qsizetype i = 1; i < self.m_tokens.size(); ++i) {
        const Token& tk = self.m_tokens[i];
        switch (tk.kind) {
        case Token::Kind::Key:
            segments.append(escapePointerSegment(tk.key));
            break;
        case Token::Kind::Index:
            segments.append(QString::number(tk.index));
            break;
        default:                    // unsupported for AsPathList
            return QJsonValue(QJsonValue::Undefined);
        }
    }
    return "/"_L1 + segments.join(u'/' );     // single pointer string
}


// ─────────────────────────────────────────────────────────────────────
//  evalStandard  – raw evaluation without the “AsPathList” post‑step
//      (moved out of evaluate() so both helpers can share the logic)
// ─────────────────────────────────────────────────────────────────────
namespace detail {

// ------------------------------------------------------------------
// 1.  Fan-out: apply one token to every value in 'src'
//     – handles the Recursive fast-path internally
// ------------------------------------------------------------------
// completely replaces the previous implementation
[[nodiscard]] inline QJsonArray fanOut(const JSONPath& self,
                                       const JSONPath::Token& tk,
                                       const QJsonArray& src)
{
    QJsonArray dst;
    for (const auto& v : src) {
        const QJsonArray seg = self.evaluateToken(tk, v);   // keep it alive
        for (const auto& e : seg)
            dst.append(e);
    }
    return dst;
}

// ------------------------------------------------------------------
// 2.  Did this token multiply the result set?
// ------------------------------------------------------------------
[[nodiscard]] constexpr bool addsMultiplicity(const JSONPath::Token& tk) noexcept
{
    using enum JSONPath::Token::Kind;
    return tk.kind != Key && tk.kind != Index;
}

// ------------------------------------------------------------------
// 3.  Collapse the working set into a single QJsonValue
// ------------------------------------------------------------------
[[nodiscard]] inline QJsonValue squash(QJsonArray arr, bool multi)
{
    if (arr.isEmpty())          return QJsonValue(QJsonValue::Undefined);
    if (!multi && arr.size()==1) return arr.first();
    return arr;
}

// ------------------------------------------------------------------
// 4.  Apply the trailing function (.length(), .min(), .max())
// ------------------------------------------------------------------
[[nodiscard]] QJsonValue applyTrailing(JSONPath::FunctionType fn,
                                       const QJsonValue& v)
{
    using enum JSONPath::FunctionType;

    switch (fn) {
    case None:   return v;

    case Length:
        if (v.isArray())  return v.toArray().size();
        if (v.isObject()) return v.toObject().size();
        return 0;

    case Min:
    case Max:
        if (!v.isArray()) return QJsonValue(QJsonValue::Undefined);
        {
            const auto arr = v.toArray();
            bool first=true; double best=0.0;
            for (const auto& e : arr) {
                if (!e.isDouble()) continue;
                const double d = e.toDouble();
                if (first || (fn==Min ? d<best : d>best))
                    best = d, first = false;
            }
            return first ? QJsonValue(QJsonValue::Undefined)
                         : QJsonValue(best);
        }
    }
    std::unreachable();
}

} // namespace detail
// ──────────────────────────────────────────────────────────────────────────────


// ─────────────────────────────────────────────────────────────────────
//  refactored evalStandard
// ─────────────────────────────────────────────────────────────────────
QJsonValue JSONPath::evalStandard(const JSONPath& self,
                                  const QJsonValue& root)
{
    if (!self.m_valid || self.m_tokens.isEmpty())
        return QJsonValue(QJsonValue::Undefined);

    QJsonArray working{root};
    bool multi = false;

    for (qsizetype i = 1; i < self.m_tokens.size() && !working.isEmpty(); ++i)
    {
        const Token& tk = self.m_tokens[i];
        working = detail::fanOut(self, tk, working);
        if (working.isEmpty())
            return QJsonValue(QJsonValue::Undefined);

        multi |= detail::addsMultiplicity(tk);
    }

    QJsonValue collapsed = detail::squash(std::move(working), multi);
    return detail::applyTrailing(self.m_func, collapsed);
}

QString JSONPath::escapePointerSegment(const QString& seg)
{
    QString out;
    out.reserve(seg.size());
    for (const QChar c : seg) {
        if (c == u'~') out += "~0"_L1;
        else if (c == u'/') out += "~1"_L1;
        else out += c;
    }
    return out;
}
