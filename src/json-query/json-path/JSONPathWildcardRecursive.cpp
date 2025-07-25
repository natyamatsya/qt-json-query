#include "json-query/json-path/JSONPathWildcardRecursive.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/internal/IterativeRecursiveDescent.hpp"
#include <QDebug>

namespace json_query::json_path::detail {

using json_query::json_path::internal::ContainerCursor;
using json_query::json_path::internal::ResultStreamer;
using internal::acquirePooledArray;
using internal::IterativeRecursiveDescent;

// ---------------------------------------------------------------------------
//  Internal implementation helpers
// ---------------------------------------------------------------------------

static std::expected<QJsonArray, EvalError> __wildcardObjectImpl(const QJsonObject& obj)
{
    // Use pooled array to reduce allocations
    auto pooledArray = acquirePooledArray();
    QJsonArray& out = *pooledArray;
    
    // Use ContainerCursor for optimized, zero-copy iteration
    auto cursor = ContainerCursor::object(obj);
    for (const auto& value : cursor) {
        out.append(value);
    }
    
    // Move result to avoid copying
    QJsonArray finalResult = std::move(out);
    return finalResult;
}

static void __evaluateRecursiveImplStreaming(const QJsonValue& value, const ResultStreamer& streamer)
{
    // Use iterative implementation to reduce call stack memory overhead
    auto result = IterativeRecursiveDescent::evaluateIterative(value, streamer);
    if (!result) {
        // Handle error - for streaming, we can't propagate errors easily
        // This maintains backward compatibility with the original streaming interface
        qWarning() << "Iterative recursive descent failed with error:" << static_cast<int>(result.error());
    }
}

static std::expected<QJsonArray, EvalError> __evaluateRecursiveImpl(const QJsonValue& value)
{
    // Use iterative implementation with array pooling for better memory efficiency
    return IterativeRecursiveDescent::evaluateIterativeArray(value);
}

// ---------------------------------------------------------------------------
//  Public wildcard evaluation API
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> wildcardObject(const QJsonObject& obj)
{
    return __wildcardObjectImpl(obj);
}

std::expected<QJsonArray, EvalError> wildcardArray(const QJsonArray& arr)
{
    // Use pooled array to reduce allocations
    auto pooledArray = acquirePooledArray();
    QJsonArray& out = *pooledArray;
    
    // Use ContainerCursor for optimized, zero-copy iteration
    auto cursor = ContainerCursor::array(arr);
    for (const auto& item : cursor) {
        out.append(item);
    }
    
    // Move result to avoid copying
    QJsonArray finalResult = std::move(out);
    return finalResult;
}

// ---------------------------------------------------------------------------
//  Public recursive evaluation API
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, int /*unused*/)
{
    return __evaluateRecursiveImpl(value);
}

} // namespace json_query::json_path::detail
