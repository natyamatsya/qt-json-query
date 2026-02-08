// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

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
    using json_query::literals::operator""_qt_s;

    // allOf: must match all schemas — pass tracker directly (all sub-schemas contribute)
    for (std::size_t i{0}; i < node.allOf.size(); ++i)
    {
        if (!ctx.shouldContinue())
            break;
        validateNode(ctx,
                     ctx.schema.nodeAt(node.allOf[i]),
                     instance,
                     instancePath,
                     schemaPath + u"/allOf/"_qt_s + QString::number(i));
    }

    // anyOf: must match at least one schema — merge tracker from ALL matching sub-schemas
    if (!node.anyOf.empty())
    {
        bool anyValid{false};
        for (std::size_t i{0}; i < node.anyOf.size(); ++i)
        {
            ValidationResult  tempResult{};
            EvaluationTracker tempTracker{};
            ValidateContext   tempCtx{ctx.schema, tempResult, true, ctx.tracker ? &tempTracker : nullptr};
            validateNode(tempCtx,
                         ctx.schema.nodeAt(node.anyOf[i]),
                         instance,
                         instancePath,
                         schemaPath + u"/anyOf/"_qt_s + QString::number(i));
            if (tempResult.isValid())
            {
                anyValid = true;
                if (ctx.tracker)
                    ctx.tracker->mergeFrom(tempTracker);
                if (!ctx.tracker)
                    break; // No tracking needed — early exit
            }
        }
        if (!anyValid)
        {
            ctx.result.addError(instancePath,
                                schemaPath + u"/anyOf"_qt_s,
                                u"Value does not match any schema in anyOf"_qt_s,
                                EvalError::AnyOfFailed);
        }
    }

    // oneOf: must match exactly one schema — merge tracker from the single match
    if (!node.oneOf.empty())
    {
        int               matchCount{0};
        EvaluationTracker matchedTracker{};
        for (std::size_t i{0}; i < node.oneOf.size(); ++i)
        {
            ValidationResult  tempResult{};
            EvaluationTracker tempTracker{};
            ValidateContext   tempCtx{ctx.schema, tempResult, true, ctx.tracker ? &tempTracker : nullptr};
            validateNode(tempCtx,
                         ctx.schema.nodeAt(node.oneOf[i]),
                         instance,
                         instancePath,
                         schemaPath + u"/oneOf/"_qt_s + QString::number(i));
            if (tempResult.isValid())
            {
                ++matchCount;
                matchedTracker = std::move(tempTracker);
                if (matchCount > 1)
                    break; // Already failed
            }
        }
        if (matchCount == 1 && ctx.tracker)
            ctx.tracker->mergeFrom(matchedTracker);

        if (matchCount != 1)
        {
            const auto msg{matchCount == 0 ? u"Value does not match any schema in oneOf"_qt_s
                                           : u"Value matches more than one schema in oneOf"_qt_s};
            ctx.result.addError(instancePath, schemaPath + u"/oneOf"_qt_s, msg, EvalError::OneOfFailed);
        }
    }

    // not: must not match (does NOT contribute evaluated properties)
    if (node.notSchema)
    {
        ValidationResult tempResult{};
        ValidateContext  tempCtx{ctx.schema, tempResult, true};
        validateNode(tempCtx, ctx.schema.nodeAt(*node.notSchema), instance, instancePath, schemaPath + u"/not"_qt_s);
        if (tempResult.isValid())
        {
            ctx.result.addError(
                instancePath, schemaPath + u"/not"_qt_s, u"Value matches schema in not"_qt_s, EvalError::NotFailed);
        }
    }

    // if/then/else — matching branch passes tracker through
    if (node.ifSchema)
    {
        ValidationResult  ifResult{};
        EvaluationTracker ifTracker{};
        ValidateContext   ifCtx{ctx.schema, ifResult, true, ctx.tracker ? &ifTracker : nullptr};
        validateNode(ifCtx, ctx.schema.nodeAt(*node.ifSchema), instance, instancePath, schemaPath + u"/if"_qt_s);

        if (ifResult.isValid())
        {
            // Merge if-branch evaluated properties
            if (ctx.tracker)
                ctx.tracker->mergeFrom(ifTracker);

            if (node.thenSchema)
            {
                validateNode(
                    ctx, ctx.schema.nodeAt(*node.thenSchema), instance, instancePath, schemaPath + u"/then"_qt_s);
            }
        }
        else if (node.elseSchema)
        {
            validateNode(ctx, ctx.schema.nodeAt(*node.elseSchema), instance, instancePath, schemaPath + u"/else"_qt_s);
        }
    }
}

} // namespace json_query::json_schema::internal
