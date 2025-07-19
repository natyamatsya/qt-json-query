#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/PathEvaluator.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"

#include <vector>
#include <deque>

namespace json_query {

using json_path::internal::ContainerCursor;
using json_path::Slice;
using json_path::Token;
using json_path::Error;
using json_path::FilterFn;

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
    json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_option, m_func};
    return json_path::detail::evaluate(ctx, *this, root);
}

// ─────────────────────────────────────────────────────────────────────
//  evaluateToken – apply a single Token to one QJsonValue
//      * Returns an array with all matches (empty ⇒ no match).
//      * Fast‑path exits keep nesting shallow.
// ─────────────────────────────────────────────────────────────────────
// -----------------------------------------------------------------------------
// Fast dispatch table: Kind → evaluator
// -----------------------------------------------------------------------------
inline QJsonArray JSONPath::evaluateToken(const Token& tk,
                                          const QJsonValue& v) const
{
    using enum Token::Kind;
    // NB: keep the order in sync with the enum!
    constexpr std::array<
        QJsonArray (*)(const json_query::JSONPath&, const Token&, const QJsonValue&), 6> lut = {
        json_path::detail::eval<Key>,
        json_path::detail::eval<Index>,
        json_path::detail::eval<Slice>,
        json_path::detail::eval<Wildcard>,
        json_path::detail::eval<Recursive>,
        json_path::detail::eval<Filter>
    };

    static_assert(lut.size() == 6, "Update the LUT when adding token kinds");
    if (static_cast<std::size_t>(tk.kind) >= lut.size())
        return {};

    return lut[static_cast<std::size_t>(tk.kind)](*this, tk, v);
}

QJsonArray JSONPath::evaluateAll(const QJsonDocument &document) const
{
    const QJsonValue root = document.isArray() ? QJsonValue(document.array())
                                               : QJsonValue(document.object());
    json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_option, m_func};
    return json_path::detail::evaluateAll(ctx, *this, root);
}

QJsonArray JSONPath::evaluateAll(const QJsonValue &value) const
{
    json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_option, m_func};
    return json_path::detail::evaluateAll(ctx, *this, value);
}

// ===================================================================
//  Wild‑cards  (object / array) – used by the * token
// ===================================================================
QJsonArray JSONPath::wildcardObject(const QJsonObject& obj) const
{
    return json_path::detail::wildcardObject(*this, obj);
}

QJsonArray JSONPath::wildcardArray(const QJsonArray& arr) const
{
    return json_path::detail::wildcardArray(*this, arr);
}

// ===================================================================
//  Recursive‑descent – collect *all* descendant containers
// ===================================================================
QJsonArray JSONPath::evaluateRecursive(const QJsonValue& value,
                                       int unused) const
{
    return json_path::detail::evaluateRecursive(*this, value, unused);
}

// ===================================================================
//  Slice helper + negative‑index helper  (already declared in header)
// ===================================================================
QJsonArray JSONPath::evalSlice(const QJsonArray& array,
                               const json_path::Slice& s) const
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
    QString out;  out.reserve(seg.size()*2);  // worst case "~"→"~0", "/"→"~1"

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

QJsonValue JSONPath::evalAsPathList(const json_query::JSONPath& self,
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

} // namespace json_query

// ─────────────────────────────────────────────────────────────────────
//  evalStandard  – raw evaluation without the "AsPathList" post‑step
//      (moved out of evaluate() so both helpers can share the logic)
// ─────────────────────────────────────────────────────────────────────
namespace json_query::json_path::detail {

// ------------------------------------------------------------------
// 1.  Fan-out: apply one token to every value in 'src'
//     – handles the Recursive fast-path internally
// ------------------------------------------------------------------
// completely replaces the previous implementation
QJsonArray fanOut(const json_query::JSONPath& self,
                  const Token& tk,
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
bool addsMultiplicity(const Token& tk)
{
    using enum Token::Kind;
    return tk.kind != Key && tk.kind != Index;
}

// ------------------------------------------------------------------
// 3.  Collapse the working set into a single QJsonValue
// ------------------------------------------------------------------
QJsonValue squash(QJsonArray arr, bool multi)
{
    if (arr.isEmpty())          return QJsonValue(QJsonValue::Undefined);
    if (!multi && arr.size()==1) return arr.first();
    return arr;
}

// ------------------------------------------------------------------
// 4.  Apply the trailing function (.length(), .min(), .max())
// ------------------------------------------------------------------
QJsonValue applyTrailing(json_path::FunctionType fn,
                        const QJsonValue& v)
{
    using enum json_path::FunctionType;

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

} // namespace json_query::json_path::detail

namespace json_query {

// ─────────────────────────────────────────────────────────────────────
//  refactored evalStandard
// ─────────────────────────────────────────────────────────────────────
QJsonValue JSONPath::evalStandard(const json_query::JSONPath& self,
                                  const QJsonValue& root)
{
    if (!self.m_valid || self.m_tokens.isEmpty())
        return QJsonValue(QJsonValue::Undefined);

    QJsonArray working{root};
    bool multi = false;

    for (qsizetype i = 1; i < self.m_tokens.size() && !working.isEmpty(); ++i)
    {
        const json_path::Token& tk = self.m_tokens[i];
        working = json_path::detail::fanOut(self, tk, working);
        if (working.isEmpty())
            return QJsonValue(QJsonValue::Undefined);

        multi |= json_path::detail::addsMultiplicity(tk);
    }

    QJsonValue collapsed = json_path::detail::squash(std::move(working), multi);
    return json_path::detail::applyTrailing(self.m_func, collapsed);
}

} // namespace json_query
