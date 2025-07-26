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
std::expected<QJsonArray, EvalError> eval<Token::Kind::Key>(const PathEvalCtx& /*ctx*/,
                                                            const Token& tk,
                                                            const QJsonValue& v)
{
    // Monadic approach: check if object, then extract key if present
    auto extractFromObject = [&tk](const QJsonObject& obj) -> std::optional<QJsonArray> {
        if (obj.contains(tk.key)) {
            QJsonArray result;
            result.append(obj[tk.key]);
            return result;
        }
        return std::nullopt;
    };

    auto asObject = [&v]() -> std::optional<QJsonObject> {
        return v.isObject() ? std::make_optional(v.toObject()) : std::nullopt;
    };

    return asObject()
        .and_then(extractFromObject)
        .value_or(QJsonArray{});
}

// --- Index -----------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> eval<Token::Kind::Index>(const PathEvalCtx& ctx,
                                                              const Token& tk,
                                                              const QJsonValue& v)
{
    // RFC 9535 compliance: "Nothing is selected from a value that is not an array"
    if (!v.isArray()) {
        return QJsonArray{}; // Empty result for non-arrays (not an error per RFC 9535)
    }
    
    const QJsonArray arr = v.toArray(); // Create copy to avoid iterator invalidation
    const int idx = normalizeIndex(tk.index, arr.size());
    
    // RFC 9535 compliance: "Nothing is selected, and it is not an error, if the index lies outside the range of the array"
    if (idx < 0 || idx >= arr.size()) {
        return QJsonArray{}; // Empty result for out-of-range (not an error per RFC 9535)
    }
    
    QJsonArray out;
    out.append(arr[idx]);
    return out;
}

// --- Slice -----------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> eval<Token::Kind::Slice>(const PathEvalCtx& /*ctx*/,
                                                              const Token& tk,
                                                              const QJsonValue& v)
{
    // Monadic approach: extract array and apply slice if present
    auto asArray = [&v]() -> std::optional<QJsonArray> {
        return v.isArray() ? std::make_optional(v.toArray()) : std::nullopt;
    };

    return asArray()
        .and_then([&tk](const QJsonArray& arr) -> std::optional<std::expected<QJsonArray, EvalError>> {
            return std::make_optional(evalSlice(arr, tk.slice));
        })
        .value_or(std::expected<QJsonArray, EvalError>{QJsonArray{}}); // Empty result for non-arrays (not an error in JSONPath)
}

// --- Wildcard --------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> eval<Token::Kind::Wildcard>(const PathEvalCtx& /*ctx*/,
                                                                  const Token&,
                                                                  const QJsonValue& v)
{
    // Monadic approach: transform value to appropriate wildcard result based on type
    auto processAsObject = [&v]() -> std::optional<std::expected<QJsonArray, EvalError>> {
        return v.isObject() ? std::make_optional(wildcardObject(v.toObject())) : std::nullopt;
    };

    auto processAsArray = [&v]() -> std::optional<std::expected<QJsonArray, EvalError>> {
        return v.isArray() ? std::make_optional(wildcardArray(v.toArray())) : std::nullopt;
    };

    // Use monadic chaining to try object first, then array, then return empty
    return processAsObject()
        .or_else(processAsArray)
        .value_or(std::expected<QJsonArray, EvalError>{QJsonArray{}}); // Empty result for primitives
}

// --- Recursive -------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> eval<Token::Kind::Recursive>(const PathEvalCtx& /*ctx*/,
                                                                   const Token&,
                                                                   const QJsonValue& v)
{
    return evaluateRecursive(v, 0);
}

// --- Filter ----------------------------------------------------------------
template<>
std::expected<QJsonArray, EvalError> eval<Token::Kind::Filter>(const PathEvalCtx& ctx,
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
std::expected<QJsonArray, EvalError> eval<Token::Kind::KeyList>(const PathEvalCtx& /*ctx*/,
                                                                const Token& tk,
                                                                const QJsonValue& v)
{
    // Monadic approach: extract keys from object if present, build result object
    auto extractKeysFromObject = [&tk](const QJsonObject& obj) -> std::optional<QJsonArray> {
        const QStringList keys = tk.key.split(u'\n');
        QJsonObject selection;
        
        for (const QString& key : keys) {
            if (obj.contains(key)) {
                selection.insert(key, obj[key]);
            }
        }
        
        if (selection.isEmpty()) {
            return std::nullopt;
        }
        
        QJsonArray result;
        result.append(selection);
        return result;
    };

    auto asObject = [&v]() -> std::optional<QJsonObject> {
        return v.isObject() ? std::make_optional(v.toObject()) : std::nullopt;
    };

    return asObject()
        .and_then(extractKeysFromObject)
        .value_or(QJsonArray{});
}

} // namespace json_query::json_path::detail
