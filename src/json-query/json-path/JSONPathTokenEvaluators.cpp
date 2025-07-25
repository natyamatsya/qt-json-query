#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"  // normalizeIndex, evalSlice
#include "json-query/json-path/JSONPathEvalError.hpp"  // EvalError
#include "json-query/json-path/internal/ContainerCursor.hpp"  // ContainerCursor for optimized iteration
#include "json-query/json-path/internal/ContextAwareContainerCursor.hpp"  // ContextAwareContainerCursor for context-aware iteration
#include <expected>

namespace json_query::json_path::detail {

using json_query::json_path::internal::ContainerCursor;

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
    return evaluateRecursive(v, 0);
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
        
        if (useContextFilter) {
            // Use ContextAwareContainerCursor for context-aware filter evaluation with zero-copy iteration
            auto cursor = internal::makeSimpleContextCursor(arr, ctx.rootDocument, v);
            const auto& contextFilterFn = ctx.contextFilters[tk.contextFilterId];
            
            for (const auto& [item, context] : cursor) {
                bool pass = contextFilterFn(item, context.rootDocument());
                if (pass) {
                    out.append(item);
                }
            }
        } else {
            // Use ContainerCursor for optimized, zero-copy array iteration during regular filter evaluation
            auto cursor = ContainerCursor::array(arr);
            for (const auto& item : cursor) {
                bool pass = false;
                if (tk.filterId >= ctx.filters.size()) continue;
                const auto& filterFn = ctx.filters[tk.filterId];
                pass = filterFn(item);
                if (pass)
                    out.append(item);
            }
        }
    } else if (v.isObject()) {
        const QJsonObject obj = v.toObject(); // Create proper copy to avoid iterator invalidation
        
        if (useContextFilter) {
            // Use ContextAwareContainerCursor for context-aware filter evaluation with zero-copy iteration
            auto cursor = internal::makeSimpleContextCursor(obj, ctx.rootDocument, v);
            const auto& contextFilterFn = ctx.contextFilters[tk.contextFilterId];
            
            for (const auto& [val, context] : cursor) {
                bool pass = contextFilterFn(val, context.rootDocument());
                if (pass) {
                    out.append(val);
                }
            }
        } else {
            // Use ContainerCursor for optimized, zero-copy object iteration during regular filter evaluation
            auto cursor = ContainerCursor::object(obj);
            for (const auto& val : cursor) {
                bool pass = false;
                if (tk.filterId >= ctx.filters.size()) continue;
                const auto& filterFn = ctx.filters[tk.filterId];
                pass = filterFn(val);
                if (pass)
                    out.append(val);
            }
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
