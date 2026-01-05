// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/ValidationHelpers.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

#include <QJsonArray>
#include <QString>
#include <ranges>

namespace json_query::json_schema::internal
{

/**
 * @brief Validate array constraints (minItems, maxItems, uniqueItems, prefixItems, items, contains)
 *
 * @param validateNode Callback for recursive validation of array items
 */
inline void validateArray(ValidateContext&    ctx,
                          const ObjectSchema& node,
                          const QJsonArray&   arr,
                          const QString&      instancePath,
                          const QString&      schemaPath,
                          ValidateNodeFn&     validateNode)
{
    using json_query::literals::operator""_qt_s;

    const auto size{static_cast<std::size_t>(arr.size())};

    if (node.minItems && size < *node.minItems)
    {
        const auto msg{QString(u"Array has %1 items, minimum is %2").arg(size).arg(*node.minItems)};
        ctx.result.addError(instancePath, schemaPath + u"/minItems"_qt_s, msg, EvalError::MinItemsViolation);
    }

    if (node.maxItems && size > *node.maxItems)
    {
        const auto msg{QString(u"Array has %1 items, maximum is %2").arg(size).arg(*node.maxItems)};
        ctx.result.addError(instancePath, schemaPath + u"/maxItems"_qt_s, msg, EvalError::MaxItemsViolation);
    }

    if (node.uniqueItems && size > 1)
    {
        // Check for duplicate items
        for (int i = 0; i < arr.size() && ctx.shouldContinue(); ++i)
        {
            for (int j = i + 1; j < arr.size(); ++j)
            {
                if (jsonValuesEqual(arr[i], arr[j]))
                {
                    ctx.result.addError(instancePath,
                                        schemaPath + u"/uniqueItems"_qt_s,
                                        u"Array items are not unique"_qt_s,
                                        EvalError::UniqueItemsViolation);
                    break;
                }
            }
        }
    }

    // Validate prefixItems
    for (const auto [i, schemaIndex] : std::views::enumerate(node.prefixItems))
    {
        if (i >= arr.size() || !ctx.shouldContinue())
            break;
        const auto itemPath{instancePath + u"/"_qt_s + QString::number(i)};
        const auto itemSchemaPath{schemaPath + u"/prefixItems/"_qt_s + QString::number(i)};
        validateNode(ctx, ctx.schema.nodeAt(schemaIndex), arr[static_cast<int>(i)], itemPath, itemSchemaPath);
    }

    // Validate items (for elements after prefixItems)
    if (node.items)
    {
        const auto startIndex{node.prefixItems.size()};
        for (const auto [i, item] : std::views::enumerate(arr))
        {
            if (static_cast<std::size_t>(i) < startIndex)
                continue;
            if (!ctx.shouldContinue())
                break;
            const auto itemPath{instancePath + u"/"_qt_s + QString::number(i)};
            validateNode(ctx, ctx.schema.nodeAt(*node.items), item, itemPath, schemaPath + u"/items"_qt_s);
        }
    }

    // Validate contains
    if (node.contains)
    {
        bool found{false};
        for (const auto [i, item] : std::views::enumerate(arr))
        {
            ValidationResult tempResult{};
            ValidateContext  tempCtx{ctx.schema, tempResult, true};
            validateNode(tempCtx,
                         ctx.schema.nodeAt(*node.contains),
                         item,
                         instancePath + u"/"_qt_s + QString::number(i),
                         schemaPath + u"/contains"_qt_s);
            if (tempResult.isValid())
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            ctx.result.addError(instancePath,
                                schemaPath + u"/contains"_qt_s,
                                u"Array does not contain required item"_qt_s,
                                EvalError::ContainsViolation);
        }
    }
}

} // namespace json_query::json_schema::internal
