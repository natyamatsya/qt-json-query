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
    if (!v.isObject()) {
        return out; // Empty result for non-objects (not an error in JSONPath)
    }
    const auto obj = v.toObject();
    if (obj.contains(tk.key))
        out.append(obj[tk.key]);
    return out;
}

template<>
QJsonArray eval<Token::Kind::Key>(const PathEvalCtx& ctx,
                                  const Token& tk,
                                  const QJsonValue& v)
{
    auto result = evalExpected<Token::Kind::Key>(ctx, tk, v);
    return result.value_or(QJsonArray{});
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
    
    const auto arr = v.toArray();
    const int idx = normalizeIndex(tk.index, arr.size());
    if (idx < 0 || idx >= arr.size()) {
        return std::unexpected(EvalError::IndexOutOfRange);
    }
    
    QJsonArray out;
    out.append(arr[idx]);
    return out;
}

// Legacy wrapper for backward compatibility
template<>
QJsonArray eval<Token::Kind::Index>(const PathEvalCtx& ctx,
                                    const Token& tk,
                                    const QJsonValue& v)
{
    auto result = evalExpected<Token::Kind::Index>(ctx, tk, v);
    return result.value_or(QJsonArray{});
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

template<>
QJsonArray eval<Token::Kind::Slice>(const PathEvalCtx& ctx,
                                    const Token& tk,
                                    const QJsonValue& v)
{
    auto result = evalExpected<Token::Kind::Slice>(ctx, tk, v);
    return result.value_or(QJsonArray{});
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

template<>
QJsonArray eval<Token::Kind::Wildcard>(const PathEvalCtx& ctx,
                                       const Token& tk,
                                       const QJsonValue& v)
{
    auto result = evalExpected<Token::Kind::Wildcard>(ctx, tk, v);
    return result.value_or(QJsonArray{});
}

// --- Recursive -------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Recursive>(const PathEvalCtx& /*ctx*/,
                                                                           const Token&,
                                                                           const QJsonValue& v)
{
    return evaluateRecursive(v);
}

template<>
QJsonArray eval<Token::Kind::Recursive>(const PathEvalCtx& ctx,
                                        const Token& tk,
                                        const QJsonValue& v)
{
    auto result = evalExpected<Token::Kind::Recursive>(ctx, tk, v);
    return result.value_or(QJsonArray{});
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
        for (const auto& item : v.toArray()) {
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
        for (auto it = v.toObject().begin(); it != v.toObject().end(); ++it) {
            const auto& val = it.value();
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

template<>
QJsonArray eval<Token::Kind::Filter>(const PathEvalCtx& ctx,
                                     const Token& tk,
                                     const QJsonValue& v)
{
    auto result = evalExpected<Token::Kind::Filter>(ctx, tk, v);
    return result.value_or(QJsonArray{});
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

template<>
QJsonArray eval<Token::Kind::KeyList>(const PathEvalCtx& ctx,
                                      const Token& tk,
                                      const QJsonValue& v)
{
    auto result = evalExpected<Token::Kind::KeyList>(ctx, tk, v);
    return result.value_or(QJsonArray{});
}

} // namespace json_query::json_path::detail
