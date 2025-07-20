#include "json-query/json-path/JSONPathEvaluate.hpp"  // normalizeIndex, evalSlice
#include <expected>
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathPointerConversion.hpp"

#include <array>
#include <deque>
#include <QSet>
#include <QJsonDocument>
#include "json-query/json-path/internal/QtHash.hpp"

#include <QStringList>
#include <QString>
#include <QDebug>

namespace json_query::json_path::detail {

// --------------------------------------------------------------
// Basic helpers (free versions copied from legacy JSONPath.cpp)
// --------------------------------------------------------------
int normalizeIndex(int idx, int size)
{
    return idx < 0 ? size + idx : idx;
}

QJsonArray evalSlice(const QJsonArray& array, const Slice& s)
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

// ---------------------------------------------------------------------------
//  Wildcard and recursive helpers (moved from JSONPathEvaluate.cpp)
// ---------------------------------------------------------------------------
namespace {
QJsonArray __wildcardObjectImpl(const QJsonObject& obj)
{
    QJsonArray out;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        out.append(it.value());
    return out;
}

QJsonArray __evaluateRecursiveImpl(const QJsonValue& value)
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
        out.append(cur);

        if (cur.isObject()) {
            const QJsonObject obj = cur.toObject();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                const QJsonValue& child = it.value();
                if (child.isArray() || child.isObject())
                    queue.push_back(child);
            }
        } else {
            const QJsonArray arr = cur.toArray();
            for (const auto& child : arr)
                if (child.isArray() || child.isObject())
                    queue.push_back(child);
        }
    }
    return out;
}
} // anonymous

QJsonArray wildcardObject(const QJsonObject& obj)
{
    return __wildcardObjectImpl(obj);
}

QJsonArray wildcardArray(const QJsonArray& arr)
{
    return arr; // shallow copy
}

QJsonArray evaluateRecursive(const QJsonValue& value, int /*unused*/)
{
    return __evaluateRecursiveImpl(value);
}

} // namespace json_query::json_path::detail

namespace json_query::json_path::detail {

// ---------------------------------------------------------------------------
//  Token dispatcher (ex-JSONPath::evaluateToken)
// ---------------------------------------------------------------------------
QJsonArray evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    using enum Token::Kind;
    constexpr std::array<QJsonArray (*)(const PathEvalCtx&, const Token&, const QJsonValue&), 7> lut = {
        eval<Key>, eval<KeyList>, eval<Index>, eval<Slice>, eval<Wildcard>, eval<Recursive>, eval<Filter>
    };

    if (static_cast<size_t>(tk.kind) >= lut.size())
        return {};

    return lut[static_cast<size_t>(tk.kind)](ctx, tk, v);
}

// ---------------------------------------------------------------------------
//  Fan-out helper (adapted from legacy implementation)
// ---------------------------------------------------------------------------
QJsonArray fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src)
{
    QJsonArray dst;
    for (const auto& v : src) {
        const QJsonArray seg = evaluateToken(ctx, tk, v);
        qDebug() << "[fanOut] kind=" << static_cast<int>(tk.kind) << "srcType"
                 << v.type() << "seg size=" << seg.size();
        for (const auto& e : seg)
            dst.append(e);
    }
    return dst;
}

// ---------------------------------------------------------------------------
//  Utility helpers reused by evalStandard
// ---------------------------------------------------------------------------
static bool addsMultiplicity(const Token& tk)
{
    using enum Token::Kind;
    return tk.kind != Key && tk.kind != KeyList && tk.kind != Index;
}

static QJsonValue squash(QJsonArray arr, bool multi)
{
    if (arr.isEmpty())
        return multi ? QJsonArray{} : QJsonValue(QJsonValue::Undefined);
    if (!multi && arr.size()==1) return arr.first();
    return arr;
}

static QJsonValue applyTrailing(json_path::FunctionType fn, const QJsonValue& v)
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

// ---------------------------------------------------------------------------
//  evalStandard – pure variant
// ---------------------------------------------------------------------------
QJsonValue evalStandard(const PathEvalCtx& ctx, const QJsonValue& root)
{
    if (ctx.tokens.isEmpty())
        return QJsonValue(QJsonValue::Undefined);

    QJsonArray working{root};
    bool multi = false;

    using json_query::json_path::internal::qt_hash;

    for (qsizetype i = 1; i < ctx.tokens.size() && !working.isEmpty(); ++i)
    {
        const Token& tk = ctx.tokens[i];
        qDebug() << "[stage] token" << i << ": kind=" << static_cast<int>(tk.kind)
                 << "working size=" << working.size();

        bool prevRecursive = (i>0 && ctx.tokens[i-1].kind == Token::Kind::Recursive);

        if (prevRecursive && (tk.kind == Token::Kind::KeyList || tk.kind == Token::Kind::Key)) {
            // Branch-unique selection: keep the parent object if it (fully) contains the requested key(s)
            const QStringList keys = (tk.kind==Token::Kind::Key)
                                    ? QStringList{tk.key}
                                    : tk.key.split(u'\n');

            bool isLeaf = (i == ctx.tokens.size()-1);

            QJsonArray next;
            for (const auto& v : working) {
                if (!v.isObject()) continue;
                const QJsonObject obj = v.toObject();

                bool all=true;
                for (const QString& k : keys)
                    if (!obj.contains(k)) { all=false; break; }
                if (!all) continue;

                if (isLeaf) {
                    if (tk.kind == Token::Kind::KeyList) {
                        // For multi-prop leaf: return parent object only (Jayway semantics)
                        next.append(v);
                    } else {
                        // Single key leaf: value(s) first
                        for (const QString& k : keys)
                            next.append(obj.value(k));
                        // Append parent object only if it contains exactly the selected key(s)
                        if (obj.size() == keys.size())
                            next.append(v);
                    }
                } else {
                    // Non-leaf: parent first, then member values for traversal
                    next.append(v);
                    for (const QString& k : keys)
                        next.append(obj.value(k));
                }
            }

            if (next.isEmpty())
                return multi ? QJsonArray{} : QJsonValue(QJsonValue::Undefined);

            working.swap(next);
            // This does NOT add multiplicity; collapse stays at previous state

            // Deduplicate at leaf too to avoid duplicate parent objects/values
            if (isLeaf) {
                QSet<uint> seen;
                QJsonArray dedup;
                for (const auto& v2 : working) {
                    if (v2.isObject()) {
                        uint h = qt_hash(QJsonDocument(v2.toObject()).toJson());
                        if (seen.contains(h)) continue;
                        seen.insert(h);
                    } else if (v2.isArray()) {
                        uint h = qt_hash(QJsonDocument(v2.toArray()).toJson());
                        if (seen.contains(h)) continue;
                        seen.insert(h);
                    }
                    dedup.append(v2);
                }
                working.swap(dedup);
            }
        } else {
            working = fanOut(ctx, tk, working);
            bool multiAfter = multi || addsMultiplicity(tk);
            if (working.isEmpty())
                return multiAfter ? QJsonArray{} : QJsonValue(QJsonValue::Undefined);

            // Deduplicate containers after normal fan-out when preceded by Recursive
            if (prevRecursive) {
                QSet<uint> seen;
                QJsonArray dedup;
                for (const auto& v : working) {
                    if (v.isObject()) {
                        uint h = qt_hash(QJsonDocument(v.toObject()).toJson());
                        if (seen.contains(h)) continue;
                        seen.insert(h);
                    } else if (v.isArray()) {
                        uint h = qt_hash(QJsonDocument(v.toArray()).toJson());
                        if (seen.contains(h)) continue;
                        seen.insert(h);
                    }
                    dedup.append(v);
                }
                working.swap(dedup);
            }

            multi = multiAfter;
        }
    }

    QJsonValue collapsed = squash(std::move(working), multi);
    return applyTrailing(ctx.trailingFn, collapsed);
}

// ---------------------------------------------------------------------------
//  evalAsPathList – pure variant
// ---------------------------------------------------------------------------
QJsonValue evalAsPathList(const PathEvalCtx& ctx, const QJsonValue& root)
{
    Q_UNUSED(root)
    if (ctx.option != PathEvalCtx::Option::AsPathList)
        return QJsonValue(QJsonValue::Undefined);

    QStringList segs;
    const QString ptr = tokensToPointer(segs, ctx.tokens);
    if (ptr.isEmpty())
        return QJsonValue(QJsonValue::Undefined);
    return ptr;
}

// ---------------------------------------------------------------------------
//  Convenience entry points (pure)
// ---------------------------------------------------------------------------
QJsonValue evaluate(const PathEvalCtx& ctx, const QJsonValue& root)
{
    if (ctx.option == PathEvalCtx::Option::AsPathList)
        return evalAsPathList(ctx, root);
    return evalStandard(ctx, root);
}

QJsonArray evaluateAll(const PathEvalCtx& ctx, const QJsonValue& root)
{
    QJsonValue res = evaluate(ctx, root);
    if (res.isArray()) return res.toArray();
    if (res.isUndefined() || res.isNull()) return {};
    return QJsonArray{res};
}

} // namespace json_query::json_path::detail
