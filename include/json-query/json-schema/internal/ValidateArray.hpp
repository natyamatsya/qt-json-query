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
    for (std::size_t i{0}; i < node.prefixItems.size(); ++i)
    {
        if (i >= arr.size() || !ctx.shouldContinue())
            break;
        const auto itemPath{instancePath + u"/"_qt_s + QString::number(i)};
        const auto itemSchemaPath{schemaPath + u"/prefixItems/"_qt_s + QString::number(i)};
        validateNode(ctx, ctx.schema.nodeAt(node.prefixItems[i]), arr[static_cast<int>(i)], itemPath, itemSchemaPath);
    }

    // Validate items (for elements after prefixItems)
    if (node.items)
    {
        const auto startIndex{node.prefixItems.size()};
        for (int i{0}; i < arr.size(); ++i)
        {
            if (static_cast<std::size_t>(i) < startIndex)
                continue;
            if (!ctx.shouldContinue())
                break;
            const auto itemPath{instancePath + u"/"_qt_s + QString::number(i)};
            validateNode(ctx, ctx.schema.nodeAt(*node.items), arr[i], itemPath, schemaPath + u"/items"_qt_s);
        }
    }

    // Validate contains + minContains / maxContains
    if (node.contains)
    {
        const auto minC{node.minContains.value_or(1)};
        const auto maxC{node.maxContains};

        // When minContains is 0 and no maxContains, contains always passes
        if (minC == 0 && !maxC)
            return;

        std::size_t matchCount{0};
        for (int i{0}; i < arr.size(); ++i)
        {
            ValidationResult tempResult{};
            ValidateContext  tempCtx{ctx.schema, tempResult, true};
            validateNode(tempCtx,
                         ctx.schema.nodeAt(*node.contains),
                         arr[i],
                         instancePath + u"/"_qt_s + QString::number(i),
                         schemaPath + u"/contains"_qt_s);
            if (tempResult.isValid())
                ++matchCount;

            // Early exit: if we already exceed maxContains
            if (maxC && matchCount > *maxC)
                break;
        }

        if (matchCount < minC)
        {
            const auto msg{QString(u"Array contains %1 matching items, minimum is %2")
                               .arg(matchCount)
                               .arg(minC)};
            ctx.result.addError(instancePath,
                                schemaPath + u"/minContains"_qt_s,
                                msg,
                                EvalError::ContainsViolation);
        }

        if (maxC && matchCount > *maxC)
        {
            const auto msg{QString(u"Array contains %1 matching items, maximum is %2")
                               .arg(matchCount)
                               .arg(*maxC)};
            ctx.result.addError(instancePath,
                                schemaPath + u"/maxContains"_qt_s,
                                msg,
                                EvalError::ContainsViolation);
        }
    }
}

} // namespace json_query::json_schema::internal
