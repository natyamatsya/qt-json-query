// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"

#include <QJsonValue>
#include <QString>
#include <ranges>

namespace json_query::json_schema::internal
{

/**
 * @brief Validate combinators (allOf, anyOf, oneOf, not, if/then/else)
 *
 * @param validateNode Callback for recursive validation of subschemas
 */
inline void validateCombinators(ValidateContext&    ctx,
                                const ObjectSchema& node,
                                const QJsonValue&   instance,
                                const QString&      instancePath,
                                const QString&      schemaPath,
                                ValidateNodeFn&     validateNode)
{
    // allOf: must match all schemas
    for (const auto [i, schemaIndex] : std::views::enumerate(node.allOf))
    {
        if (!ctx.shouldContinue())
            break;
        validateNode(ctx,
                     ctx.schema.nodeAt(schemaIndex),
                     instance,
                     instancePath,
                     schemaPath + u"/allOf/"_qs + QString::number(i));
    }

    // anyOf: must match at least one schema
    if (!node.anyOf.empty())
    {
        bool anyValid{false};
        for (const auto [i, schemaIndex] : std::views::enumerate(node.anyOf))
        {
            ValidationResult tempResult{};
            ValidateContext  tempCtx{ctx.schema, tempResult, true};
            validateNode(tempCtx,
                         ctx.schema.nodeAt(schemaIndex),
                         instance,
                         instancePath,
                         schemaPath + u"/anyOf/"_qs + QString::number(i));
            if (tempResult.isValid())
            {
                anyValid = true;
                break;
            }
        }
        if (!anyValid)
        {
            ctx.result.addError(instancePath,
                                schemaPath + u"/anyOf"_qs,
                                u"Value does not match any schema in anyOf"_qs,
                                EvalError::AnyOfFailed);
        }
    }

    // oneOf: must match exactly one schema
    if (!node.oneOf.empty())
    {
        int matchCount{0};
        for (const auto [i, schemaIndex] : std::views::enumerate(node.oneOf))
        {
            ValidationResult tempResult{};
            ValidateContext  tempCtx{ctx.schema, tempResult, true};
            validateNode(tempCtx,
                         ctx.schema.nodeAt(schemaIndex),
                         instance,
                         instancePath,
                         schemaPath + u"/oneOf/"_qs + QString::number(i));
            if (tempResult.isValid())
            {
                ++matchCount;
                if (matchCount > 1)
                    break; // Already failed
            }
        }
        if (matchCount != 1)
        {
            const auto msg{matchCount == 0 ? u"Value does not match any schema in oneOf"_qs
                                           : u"Value matches more than one schema in oneOf"_qs};
            ctx.result.addError(instancePath, schemaPath + u"/oneOf"_qs, msg, EvalError::OneOfFailed);
        }
    }

    // not: must not match
    if (node.notSchema)
    {
        ValidationResult tempResult{};
        ValidateContext  tempCtx{ctx.schema, tempResult, true};
        validateNode(tempCtx, ctx.schema.nodeAt(*node.notSchema), instance, instancePath, schemaPath + u"/not"_qs);
        if (tempResult.isValid())
        {
            ctx.result.addError(
                instancePath, schemaPath + u"/not"_qs, u"Value matches schema in not"_qs, EvalError::NotFailed);
        }
    }

    // if/then/else
    if (node.ifSchema)
    {
        ValidationResult ifResult{};
        ValidateContext  ifCtx{ctx.schema, ifResult, true};
        validateNode(ifCtx, ctx.schema.nodeAt(*node.ifSchema), instance, instancePath, schemaPath + u"/if"_qs);

        if (ifResult.isValid())
        {
            if (node.thenSchema)
            {
                validateNode(
                    ctx, ctx.schema.nodeAt(*node.thenSchema), instance, instancePath, schemaPath + u"/then"_qs);
            }
        }
        else if (node.elseSchema)
        {
            validateNode(ctx, ctx.schema.nodeAt(*node.elseSchema), instance, instancePath, schemaPath + u"/else"_qs);
        }
    }
}

} // namespace json_query::json_schema::internal
