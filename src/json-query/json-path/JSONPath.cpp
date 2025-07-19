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
    return json_path::detail::evaluate(ctx, root);
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
    using Ctx = json_path::detail::PathEvalCtx;
    constexpr std::array<
        QJsonArray (*)(const Ctx&, const Token&, const QJsonValue&), 6> lut = {
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

    Ctx ctx{m_tokens, m_filters, m_option, m_func};
    return lut[static_cast<std::size_t>(tk.kind)](ctx, tk, v);
}

QJsonArray JSONPath::evaluateAll(const QJsonDocument &document) const
{
    const QJsonValue root = document.isArray() ? QJsonValue(document.array())
                                               : QJsonValue(document.object());
    json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_option, m_func};
    return json_path::detail::evaluateAll(ctx, root);
}

QJsonArray JSONPath::evaluateAll(const QJsonValue &value) const
{
    json_path::detail::PathEvalCtx ctx{m_tokens, m_filters, m_option, m_func};
    return json_path::detail::evaluateAll(ctx, value);
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

// (legacy evalStandard/evalAsPathList and helper functions removed – now provided
//  by PathEvaluator.cpp as pure utilities)
