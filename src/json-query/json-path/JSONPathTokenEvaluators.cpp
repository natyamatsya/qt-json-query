#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"  // normalizeIndex, evalSlice
#include "json-query/json-path/JSONPathEvalError.hpp"  // EvalError
#include <expected>

namespace json_query::json_path::detail {

// --- Key -------------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Key>(const PathEvalCtx& /*ctx*/,
                                                                     const Token& tk,
                                                                     const QJsonValue& v)
{
    QJsonArray out;
    if (!v.isObject())
        return out; // Empty result for non-objects
    const QJsonObject obj = v.toObject();
    if (obj.contains(tk.key))
        out.append(obj[tk.key]);
    return out;
}

// --- Index -----------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Index>(const PathEvalCtx& ctx,
                                                                       const Token& tk,
                                                                       const QJsonValue& v)
{
    if (!v.isArray()) {
        return std::unexpected(EvalError::TypeMismatchArray);
    }
    
    const QJsonArray arr = v.toArray(); // Create copy to avoid iterator invalidation
    const int idx = normalizeIndex(tk.index, arr.size());
    if (idx < 0 || idx >= arr.size()) {
        return std::unexpected(EvalError::IndexOutOfRange);
    }
    
    QJsonArray out;
    out.append(arr[idx]);
    return out;
}

// --- Slice -----------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Slice>(const PathEvalCtx& /*ctx*/,
                                                                       const Token& tk,
                                                                       const QJsonValue& v)
{
    if (!v.isArray()) {
        return QJsonArray{}; // Empty result for non-arrays (not an error in JSONPath)
    }
    return evalSlice(v.toArray(), tk.slice);
}

// --- Wildcard --------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Wildcard>(const PathEvalCtx& /*ctx*/,
                                                                          const Token&,
                                                                          const QJsonValue& v)
{
    if (v.isObject()) {
        return wildcardObject(v.toObject());
    } else if (v.isArray()) {
        return wildcardArray(v.toArray());
    }
    return QJsonArray{}; // Empty result for primitives
}

// --- Recursive -------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Recursive>(const PathEvalCtx& /*ctx*/,
                                                                           const Token&,
                                                                           const QJsonValue& v)
{
    return evaluateRecursive(v);
}

// --- Filter ----------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Filter>(const PathEvalCtx& ctx,
                                                                        const Token& tk,
                                                                        const QJsonValue& v)
{
    QJsonArray out;
    if (tk.filterId >= ctx.filters.size() && tk.contextFilterId >= ctx.contextFilters.size()) {
        return out;
    }

    // Determine which type of filter to use
    bool useContextFilter = (tk.contextFilterId != SIZE_MAX && tk.contextFilterId < ctx.contextFilters.size());
    
    if (v.isArray()) {
        const QJsonArray arr = v.toArray(); // Create proper copy to avoid iterator invalidation
        for (const auto& item : arr) {
            bool pass = false;
            if (useContextFilter) {
                const auto& contextFilterFn = ctx.contextFilters[tk.contextFilterId];
                pass = contextFilterFn(item, ctx.rootDocument);
            } else {
                if (tk.filterId >= ctx.filters.size()) continue;
                const auto& filterFn = ctx.filters[tk.filterId];
                pass = filterFn(item);
            }
            if (pass)
                out.append(item);
        }
    } else if (v.isObject()) {
        const QJsonObject obj = v.toObject(); // Create proper copy to avoid iterator invalidation
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            const QJsonValue val = it.value(); // Create proper copy instead of reference
            bool pass = false;
            if (useContextFilter) {
                const auto& contextFilterFn = ctx.contextFilters[tk.contextFilterId];
                pass = contextFilterFn(val, ctx.rootDocument);
            } else {
                if (tk.filterId >= ctx.filters.size()) continue;
                const auto& filterFn = ctx.filters[tk.filterId];
                pass = filterFn(val);
            }
            if (pass)
                out.append(val);
        }
    }
    return out;
}

// --- KeyList ---------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::KeyList>(const PathEvalCtx& /*ctx*/,
                                                                         const Token& tk,
                                                                         const QJsonValue& v)
{
    QJsonArray out;
    if (!v.isObject()) {
        return out; // Empty result for non-objects
    }

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
