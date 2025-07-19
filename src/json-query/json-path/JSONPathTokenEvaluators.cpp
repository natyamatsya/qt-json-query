#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPath.hpp"

namespace json_query::json_path::detail {

// --- Key -------------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Key>(const json_query::JSONPath&,
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
QJsonArray eval<Token::Kind::Index>(const json_query::JSONPath& jp,
                                    const Token& tk,
                                    const QJsonValue& v)
{
    QJsonArray out;
    if (!v.isArray()) return out;
    const auto arr = v.toArray();
    const int idx = jp.normalizeIndex(tk.index, arr.size());
    if (idx >= 0 && idx < arr.size())
        out.append(arr[idx]);
    return out;
}

// --- Slice -----------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Slice>(const json_query::JSONPath& jp,
                                    const Token& tk,
                                    const QJsonValue& v)
{
    if (v.isArray())
        return jp.evalSlice(v.toArray(), tk.slice);
    return {};
}

// --- Wildcard --------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Wildcard>(const json_query::JSONPath& jp,
                                       const Token&,
                                       const QJsonValue& v)
{
    if (v.isObject()) return jp.wildcardObject(v.toObject());
    if (v.isArray())  return jp.wildcardArray (v.toArray());
    return {};
}

// --- Recursive -------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Recursive>(const json_query::JSONPath& jp,
                                        const Token&,
                                        const QJsonValue& v)
{
    return jp.evaluateRecursive(v, 0);
}

// --- Filter ----------------------------------------------------------------
template<>
QJsonArray eval<Token::Kind::Filter>(const json_query::JSONPath& jp,
                                     const Token& tk,
                                     const QJsonValue& v)
{
    QJsonArray out;
    if (tk.filterId >= jp.m_filters.size()) return out;

    const auto& filterFn = jp.m_filters[tk.filterId];
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

} // namespace json_query::json_path::detail
