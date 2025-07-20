#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"  // normalizeIndex, evalSlice

namespace json_query::json_path::detail {

// --- Key -------------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Key>(const PathEvalCtx& /*ctx*/,
                                  const Token& tk,
                                  const QJsonValue& v)
{
    QJsonArray out;
    if (!v.isObject()) return out;
    const auto obj = v.toObject();
    if (obj.contains(tk.key))
        out.append(obj[tk.key]);
    return out;
}

// --- Index -----------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Index>(const PathEvalCtx& ctx,
                                    const Token& tk,
                                    const QJsonValue& v)
{
    QJsonArray out;
    if (!v.isArray()) return out;
    const auto arr = v.toArray();
    const int idx = normalizeIndex(tk.index, arr.size());
    if (idx >= 0 && idx < arr.size())
        out.append(arr[idx]);
    return out;
}

// --- Slice -----------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Slice>(const PathEvalCtx& /*ctx*/,
                                    const Token& tk,
                                    const QJsonValue& v)
{
    if (v.isArray())
        return evalSlice(v.toArray(), tk.slice);
    return {};
}

// --- Wildcard --------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Wildcard>(const PathEvalCtx& /*ctx*/,
                                       const Token&,
                                       const QJsonValue& v)
{
    if (v.isObject()) return wildcardObject(v.toObject());
    if (v.isArray())  return wildcardArray (v.toArray());
    return {};
}

// --- Recursive -------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Recursive>(const PathEvalCtx& /*ctx*/,
                                        const Token&,
                                        const QJsonValue& v)
{
    return evaluateRecursive(v);
}

// --- Filter ----------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Filter>(const PathEvalCtx& ctx,
                                     const Token& tk,
                                     const QJsonValue& v)
{
    QJsonArray out;
    if (tk.filterId >= ctx.filters.size()) return out;

    const auto& filterFn = ctx.filters[tk.filterId];
    if (v.isArray()) {
        for (const auto& item : v.toArray()) {
            if (filterFn(item))
                out.append(item);
        }
    } else if (v.isObject()) {
        if (filterFn(v))
            out.append(v);
    }
    return out;
}

// --- KeyList ---------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::KeyList>(const PathEvalCtx& /*ctx*/,
                                     const Token& tk,
                                     const QJsonValue& v)
{
    QJsonArray out;
    if (!v.isObject()) return out;

    const QJsonObject obj = v.toObject();
    const QStringList keys = tk.key.split(u'\n');
    QJsonObject sel;
    for (const QString& k : keys) {
        if (obj.contains(k))
            sel.insert(k, obj[k]);
    }
    if (!sel.isEmpty())
        out.append(sel);
    return out;
}

} // namespace json_query::json_path::detail
