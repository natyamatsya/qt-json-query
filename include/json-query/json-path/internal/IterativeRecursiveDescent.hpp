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
 */
struct RecursiveDescent
{
    static constexpr std::size_t kMaxResults{10000};
    static constexpr std::size_t kMaxStackDepth{100};

    /**
     * @brief Emit every value in depth-first order with safety limits.
     *
     * Used for $..* and generic recursive descent. The next token in the
     * pipeline (via fanOut) handles key/index selection from the result set.
     */
    static std::expected<QJsonArray, EvalError> evaluateAll(const QJsonValue& root)
    {
        auto  pooled{acquirePooledArray()};
        auto& result = *pooled;

        thread_local std::vector<QJsonValue> stack;
        stack.clear();
        stack.push_back(root);

        std::size_t resultCount{0};

        while (!stack.empty())
        {
            if (stack.size() > kMaxStackDepth || resultCount > kMaxResults)
                return std::unexpected(EvalError::TooComplex);

            const auto current{std::move(stack.back())};
            stack.pop_back();

            result.append(current);
            ++resultCount;

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
