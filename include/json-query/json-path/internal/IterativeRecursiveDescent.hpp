// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-path/JSONPathError.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/utils/BraceSafe.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <expected>
#include <vector>

namespace json_query::json_path::internal
{

/**
 * @brief Iterative recursive descent for JSONPath $.. evaluation.
 *
 * Uses an explicit std::vector<QJsonValue> stack with pop-emit-push
 * (no "processed" flag, no pool allocator, no pointer indirection).
 *
 * Deliberately unbounded: the traversal is iterative (no C++ call-stack
 * growth), visits each node of the already-parsed document exactly once,
 * and the result holds COW references — peak memory and output size are
 * O(document node count), i.e. bounded by input the caller already holds.
 * Qt's own JSON parser additionally caps nesting at 1024 levels.
 */
struct RecursiveDescent
{
    /**
     * @brief Emit every value in depth-first (document) order.
     *
     * Used for $..* and generic recursive descent. The next token in the
     * pipeline (via fanOut) handles key/index selection from the result set.
     * Never fails; the expected return type is kept for interface uniformity.
     */
    static std::expected<QJsonArray, EvalError> evaluateAll(const QJsonValue& root)
    {
        auto  pooled{acquirePooledArray()};
        auto& result = *pooled;

        thread_local std::vector<QJsonValue> stack;
        stack.clear();
        stack.push_back(root);

        while (!stack.empty())
        {
            const auto current{std::move(stack.back())};
            stack.pop_back();

            result.append(current);

            if (current.isObject())
            {
                const auto obj{current.toObject()};
                for (auto it{obj.end()}; it != obj.begin();)
                {
                    --it;
                    stack.push_back(it.value());
                }
            }
            else if (current.isArray())
            {
                const auto arr{asArray(current)};
                for (auto i{arr.size() - 1}; i >= 0; --i)
                    stack.push_back(arr[i]);
            }
        }

        return std::move(result);
    }
};

} // namespace json_query::json_path::internal
