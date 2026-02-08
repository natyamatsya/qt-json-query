// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPathWildcardRecursive.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/internal/IterativeRecursiveDescent.hpp"

namespace json_query::json_path::detail
{

using internal::acquirePooledArray;
using internal::RecursiveDescent;
using json_query::json_path::internal::ContainerCursor;

// ---------------------------------------------------------------------------
//  Public wildcard evaluation API
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> wildcardObject(const QJsonObject& obj)
{
    auto  pooled{acquirePooledArray()};
    auto& out = *pooled;

    auto cursor{ContainerCursor::object(obj)};
    for (const auto& value : cursor)
        out.append(value);

    return std::move(out);
}

std::expected<QJsonArray, EvalError> wildcardArray(const QJsonArray& arr)
{
    auto  pooled{acquirePooledArray()};
    auto& out = *pooled;

    auto cursor{ContainerCursor::array(arr)};
    for (const auto& item : cursor)
        out.append(item);

    return std::move(out);
}

// ---------------------------------------------------------------------------
//  Public recursive evaluation API
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, QStringView /*pathHint*/)
{
    return RecursiveDescent::evaluateAll(value);
}

std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, int /*unused*/)
{
    return RecursiveDescent::evaluateAll(value);
}

} // namespace json_query::json_path::detail
