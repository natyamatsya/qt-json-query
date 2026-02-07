// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/utils/BraceSafe.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <ranges>

namespace json_query::json_schema::internal
{

/**
 * @brief Check if two QJsonValues are deeply equal
 *
 * Used for enum/const validation. Performs recursive comparison
 * for arrays and objects.
 *
 * @param a First JSON value
 * @param b Second JSON value
 * @return true if values are equal, false otherwise
 */
[[nodiscard]] inline bool jsonValuesEqual(const QJsonValue& a, const QJsonValue& b) noexcept
{
    if (a.type() != b.type())
        return false;

    switch (a.type())
    {
    case QJsonValue::Null:
        return true;
    case QJsonValue::Bool:
        return a.toBool() == b.toBool();
    case QJsonValue::Double:
        return a.toDouble() == b.toDouble();
    case QJsonValue::String:
        return a.toString() == b.toString();
    case QJsonValue::Array:
    {
        const auto arrA{asArray(a)};
        const auto arrB{asArray(b)};
        if (arrA.size() != arrB.size())
            return false;
        for (const auto& [itemA, itemB] : std::views::zip(arrA, arrB))
            if (!jsonValuesEqual(itemA, itemB))
                return false;
        return true;
    }
    case QJsonValue::Object:
    {
        const auto objA{a.toObject()};
        const auto objB{b.toObject()};
        if (objA.size() != objB.size())
            return false;
        for (auto it = objA.begin(); it != objA.end(); ++it)
            if (!objB.contains(it.key()) || !jsonValuesEqual(it.value(), objB[it.key()]))
                return false;
        return true;
    }
    default:
        return false;
    }
}

} // namespace json_query::json_schema::internal
