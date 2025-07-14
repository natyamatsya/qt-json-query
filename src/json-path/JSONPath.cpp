// jsonpath.cpp - Using CTRE
#include "json-query/JSONPath.hpp"
#include <vector>
#include <regex>
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
} // namespace

JSONPath::Result JSONPath::create(QStringView rawPath, Option opt)
{
    QString path = rawPath.toString();
    // Extract any trailing function → updates `path` and yields `func`
    FunctionType func = FunctionType::None;
    detectTrailingFunction(path);          // make this overload return func

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
    static const QPair<QString, FunctionType> table[] = {
        {".length()", FunctionType::Length},
        {".min()",    FunctionType::Min   },
        {".max()",    FunctionType::Max   },
    };
    for (auto& p : table)
        if (path.endsWith(p.first)) { path.chop(p.first.size()); return p.second; }
    return FunctionType::None;
}

std::expected<JSONPath::Compiled, Error> JSONPath::compilePath(const QStringView sv)
{
    using Kind = Token::Kind;

    QVector<Token> tokens;
    QVector<FilterFn> filters;
    tokens.reserve(sv.count('.') + sv.count('[') + 4);

    // ── root ────────────────────────────────────────────────────────
    if (sv.empty() || (sv[0] != u'$' && sv[0] != u'@'))
        return std::unexpected(Error::MissingRoot);

    tokens.append(Token{ Kind::Key, 0, {},
                         qt_hash(sv.first(1)),
                         sv.first(1).toString(), 0 });

    qsizetype pos = 1, n = sv.size();

    auto fail = [](Error e)
        { return std::unexpected(e); };

    auto pushKey = [&](QStringView key) -> std::expected<void, Error>
    {
        if (key.isEmpty()) return fail(Error::EmptySegment);
        QString s{};// segmentToPointer(key.toString());
        tokens.append(Token{ Kind::Key, 0, {}, qt_hash(s), std::move(s), 0 });
        return {};
    };

    // ── main loop ───────────────────────────────────────────────────
    while (pos < n)
    {
        const QChar c = sv[pos];

        // dot-notation ------------------------------------------------
        if (c == u'.') {
            if (++pos >= n) return fail(Error::TrailingDot);
            const QChar next = sv[pos];

            if (next == u'.') { tokens.append(Token{Kind::Recursive}); ++pos; continue; }
            if (next == u'*') { tokens.append(Token{Kind::Wildcard});  ++pos; continue; }

            qsizetype start = pos;
            while (pos < n && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
            if (auto r = pushKey(sv.sliced(start, pos - start)); !r)
                return std::unexpected{r.error()};
            continue;
        }

        // bracket-notation -------------------------------------------
        if (c == u'[') {
            const qsizetype startB = ++pos;
            int depth = 1;
            while (pos < n && depth) {
                if   (sv[pos] == u'[') ++depth;
                else if (sv[pos] == u']') --depth;
                ++pos;
            }
            if (depth) return fail(Error::UnmatchedBracket);

            const QStringView content = sv.sliced(startB, pos - startB - 1);

            if (content == u"*") { tokens.append(Token{Kind::Wildcard}); continue; }

            if (!content.isEmpty() && content.front() == u'?') {
                auto tk = compileFilter(content.mid(1).toString(), filters);

                if (!tk) return fail(Error::UnsupportedFilter);
                tokens.append(*tk); continue;
            }

            if (content.contains(u':')) {
                tokens.append(Token{Kind::Slice,0,makeSlice(content),0u});
                continue;
            }

            if (!content.isEmpty() &&
                (content.front()==u'\''||content.front()==u'\"'))
            {
                auto qEnd = content.lastIndexOf(content.front());
                if (qEnd <= 0) return fail(Error::UnmatchedQuote);
                if (auto r = pushKey(content.sliced(1, qEnd-1)); !r)
                    return std::unexpected{r.error()};
                continue;
            }

            bool ok=false; int idx = QString{content}.toInt(&ok);
            if (ok) { tokens.append(Token{Kind::Index, idx}); continue; }

            // fallback: treat as raw key
            QString bracketSeg = "[" + QString(content) + "]";
            if (auto r = pushKey(bracketSeg); !r)
                return std::unexpected{r.error()};;
            continue;
        }

        // bare property name -----------------------------------------
        qsizetype start = pos;
        while (pos < n && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
        if (auto r = pushKey(sv.sliced(start, pos - start)); !r)
            return std::unexpected{r.error()};
    }
    return Compiled{tokens, {}};                        // success
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
    if (!m_valid || m_tokens.isEmpty())
        return QJsonValue(QJsonValue::Undefined);

    // Stage values in a worklist; start with the supplied root.
    QJsonArray working { root };
    bool multi = false;                     // did we hit any fan‑out token?

    // Skip token #0: it's always "$" or "@" referring to root itself.
    for (qsizetype i = 1; i < m_tokens.size() && !working.isEmpty(); ++i)
    {
        const Token& tk = m_tokens[i];

        // Special‑case the Recursive token to avoid one superfluous pass:
        // we replace the worklist with *all* descendants immediately.
        if (tk.kind == Token::Kind::Recursive) {
            QJsonArray next;
            for (const auto& v : working) {
                QJsonArray rec = evaluateToken(tk, v);
                for (const auto& e : rec) next.append(e);
            }
            working.swap(next);
            multi = true;
            continue;                       // next token uses the new worklist
        }

        // Normal tokens
        QJsonArray next;
        for (const auto& v : working) {
            QJsonArray seg = evaluateToken(tk, v);
            for (const auto& e : seg) next.append(e);
        }
        if (next.isEmpty())                 // short‑circuit no‑match
            return QJsonValue(QJsonValue::Undefined);

        // fan‑out?
        if (tk.kind != Token::Kind::Key && tk.kind != Token::Kind::Index)
            multi = true;

        working.swap(next);
    }

    // -----------------------------------------------------------------
    //  Collapse result + apply optional trailing function
    // -----------------------------------------------------------------
    QJsonValue result;
    if (working.isEmpty()) {
        result = QJsonValue(QJsonValue::Undefined);
    } else if (!multi && working.size() == 1) {
        result = working.first();
    } else {
        result = working;
    }

    switch (m_func)
    {
        case FunctionType::None:
            return result;

        case FunctionType::Length:
            if (result.isArray())   return result.toArray().size();
            if (result.isObject())  return result.toObject().size();
            return 0;

        case FunctionType::Min:
        case FunctionType::Max: {
            if (!result.isArray()) return QJsonValue(QJsonValue::Undefined);
            auto arr = result.toArray();
            bool first = true; double best = 0.0;
            for (const auto& v : arr) {
                if (!v.isDouble()) continue;
                double d = v.toDouble();
                if (first || (m_func==FunctionType::Min ? d<best : d>best)) {
                    best = d; first = false;
                }
            }
            return first ? QJsonValue(QJsonValue::Undefined)
                         : QJsonValue(best);
        }
    }
    std::unreachable();   // all enum cases handled
}

// ─────────────────────────────────────────────────────────────────────
//  evaluateToken – apply a single Token to one QJsonValue
//      * Returns an array with all matches (empty ⇒ no match).
//      * Fast‑path exits keep nesting shallow.
// ─────────────────────────────────────────────────────────────────────
QJsonArray JSONPath::evaluateToken(const Token& tk,
                                   const QJsonValue& v) const
{
    using enum Token::Kind;
    QJsonArray out;

    switch (tk.kind)
    {
        // ------------------------------------------------ Key
        case Key: {
            if (!v.isObject()) return out;
            const auto it = v.toObject().find(tk.key);
            if (it != v.toObject().end()) out.append(*it);
            return out;
        }

        // ------------------------------------------------ Index
        case Index: {
            if (!v.isArray()) return out;
            const auto& arr = v.toArray();
            int idx = normalizeIndex(static_cast<int>(tk.index), arr.size());
            if (idx >= 0 && idx < arr.size()) out.append(arr[idx]);
            return out;
        }

        // ------------------------------------------------ Slice
        case Slice:
            if (v.isArray())
                return evalSlice(v.toArray(), tk.slice);
            return out;

        // ------------------------------------------------ Wildcard
        case Wildcard:
            if (v.isObject()) return wildcardObject(v.toObject());
            if (v.isArray())  return wildcardArray (v.toArray());
            return out;

        // ------------------------------------------------ Recursive
        case Recursive:
            return evaluateRecursive(v, /*unused*/0);

        // ------------------------------------------------ Filter
        case Filter: {
            if (tk.filterId >= static_cast<std::size_t>(m_filters.size()))
                return out;
            const auto& pred = m_filters[tk.filterId];

            if (v.isArray()) {
                for (const auto& e : v.toArray())
                    if (pred(e)) out.append(e);
                return out;
            }
            if (pred(v)) out.append(v);
            return out;
        }
    }

    std::unreachable();
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

// ───────────────────────────────────────────────────────────────
//  compileFilter  — turns [? …] into Token{Filter,…} + lambda
//      Supports three forms:
//        1.  @.prop  <op>  value          (numeric or string, == != > < >= <=)
//        2.  @['prop'] <op> value         (same operators)
//        3.  'foo' in @['arrayProp']
//      Extend with more patterns as needed.
// ───────────────────────────────────────────────────────────────
std::optional<Token>
JSONPath::compileFilter(const QString& expr,
                        QVector<json_query::FilterFn>& out)
{
    using Kind = Token::Kind;
    using json_query::utils::to_sv;
    using json_query::utils::to_qstr;

    QString s = expr.trimmed();
    if (s.startsWith(u'?'))  s.remove(0,1);
    if (s.startsWith(u'(') && s.endsWith(u')'))
        s = s.mid(1, s.size()-2).trimmed();

    // ---------------------------------------------------  'foo' in @['tags']
    {
        constexpr auto pat = ctll::fixed_string{
            R"('\s*([^']+?)\s*'\s+in\s+@\[['\"]([^'\"]+)['\"]\])"};
        if (auto m = ctre::match<pat>(to_sv(s)))
        {
            const QString want  = to_qstr(m.template get<1>().to_view());
            const QString array = to_qstr(m.template get<2>().to_view());

            std::size_t id = out.size();
            out.push_back([want,array](const QJsonValue& j)->bool{
                auto arr = j[array];
                if (!arr.isArray()) return false;
                for (auto v: arr.toArray())
                    if (v.isString() && v.toString()==want) return true;
                return false;
            });
            return Token{Kind::Filter,0,{},0u,array,id};
        }
    }

    // ---------------------------------------------------  @.prop <op> value
    {
        constexpr auto pat = ctll::fixed_string{
            R"(@\.([\w$]+)\s*(==|!=|>=|<=|>|<)\s*(.+))"};
        if (auto m = ctre::match<pat>(to_sv(s)))
        {
            const QString prop = to_qstr(m.template get<1>().to_view());
            const QString op   = to_qstr(m.template get<2>().to_view());
            QString rhs        = to_qstr(m.template get<3>().to_view()).trimmed();

            bool  isNum   = false;
            const double num = rhs.toDouble(&isNum);

            // unquote if string
            if (!isNum && rhs.size() >=2 &&
                ((rhs.front()==u'"'&&rhs.back()==u'"') ||
                 (rhs.front()==u'\''&&rhs.back()==u'\'')))
                rhs = rhs.mid(1, rhs.size()-2);

            std::size_t id = out.size();
            out.push_back([prop,op,isNum,num,rhs](const QJsonValue& j)->bool {
                const auto v = j[prop];
                if (isNum) {
                    if (!v.isDouble()) return false;
                    double x=v.toDouble();
                    if (op=="==") return x==num;
                    if (op=="!=") return x!=num;
                    if (op==">")  return x> num;
                    if (op=="<")  return x< num;
                    if (op==">=") return x>=num;
                    if (op=="<=") return x<=num;
                    return false;
                } else {
                    const QString vs = v.toString();
                    if (op=="==") return vs==rhs;
                    if (op=="!=") return vs!=rhs;
                    return false;
                }
            });
            return Token{Kind::Filter,0,{},0u,prop,id};
        }
    }

    // ---------------------------------------------------  @['prop'] <op> value
    {
        constexpr auto pat = ctll::fixed_string{
            R"(@\[['\"]([^'\"]+)['\"]\]\s*(==|!=|>=|<=|>|<)\s*(.+))"};
        if (auto m = ctre::match<pat>(to_sv(s)))
        {
            const QString prop = to_qstr(m.template get<1>().to_view());
            const QString op   = to_qstr(m.template get<2>().to_view());
            QString rhs        = to_qstr(m.template get<3>().to_view()).trimmed();

            bool  isNum = false;
            const double num = rhs.toDouble(&isNum);

            if (!isNum && rhs.size()>=2 &&
                ((rhs.front()==u'"'&&rhs.back()==u'"') ||
                 (rhs.front()==u'\''&&rhs.back()==u'\'')))
                rhs = rhs.mid(1,rhs.size()-2);

            std::size_t id = out.size();
            out.push_back([prop,op,isNum,num,rhs](const QJsonValue& j)->bool{
                const auto v=j[prop];
                if (isNum) {
                    if (!v.isDouble()) return false;
                    double x=v.toDouble();
                    if (op=="==") return x==num;
                    if (op=="!=") return x!=num;
                    if (op==">")  return x> num;
                    if (op=="<")  return x< num;
                    if (op==">=") return x>=num;
                    if (op=="<=") return x<=num;
                    return false;
                } else {
                    const QString vs=v.toString();
                    if (op=="==") return vs==rhs;
                    if (op=="!=") return vs!=rhs;
                    return false;
                }
            });
            return Token{Kind::Filter,0,{},0u,prop,id};
        }
    }

    // ---------------------------------------------------  unsupported
    return std::nullopt;
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

    std::vector<QJsonValue> stack;
    stack.push_back(value);
    while (!stack.empty())
    {
        QJsonValue cur = stack.back();
        stack.pop_back();

        // store the container itself
        out.append(cur);

        if (cur.isObject()) {
            const QJsonObject obj = cur.toObject();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                const QJsonValue& child = it.value();
                if (child.isArray() || child.isObject())
                    stack.push_back(child);
            }
        } else {                     // array
            const QJsonArray arr = cur.toArray();
            for (const auto& child : arr)
                if (child.isArray() || child.isObject())
                    stack.push_back(child);
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
