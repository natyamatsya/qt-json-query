// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/json-pointer/JSONPointerUtils.hpp"

#include <QJsonObject>
#include <QSet>
#include <QString>

namespace json_query::json_schema::internal
{

/**
 * @brief Validate object constraints (properties, required, additionalProperties, etc.)
 *
 * @param validateNode Callback for recursive validation of property values
 */
inline void validateObject(ValidateContext&    ctx,
                           const ObjectSchema& node,
                           const QJsonObject&  obj,
                           const QString&      instancePath,
                           const QString&      schemaPath,
                           ValidateNodeFn&     validateNode)
{
    const auto size{static_cast<std::size_t>(obj.size())};

    if (node.minProperties && size < *node.minProperties)
    {
        const auto msg{QString(u"Object has %1 properties, minimum is %2").arg(size).arg(*node.minProperties)};
        ctx.result.addError(instancePath, schemaPath + u"/minProperties"_qs, msg, EvalError::MinPropertiesViolation);
    }

    if (node.maxProperties && size > *node.maxProperties)
    {
        const auto msg{QString(u"Object has %1 properties, maximum is %2").arg(size).arg(*node.maxProperties)};
        ctx.result.addError(instancePath, schemaPath + u"/maxProperties"_qs, msg, EvalError::MaxPropertiesViolation);
    }

    // Check required properties
    for (const QString& req : node.required)
    {
        if (!obj.contains(req))
        {
            const auto msg{QString(u"Required property '%1' is missing").arg(req)};
            ctx.result.addError(instancePath, schemaPath + u"/required"_qs, msg, EvalError::RequiredMissing);
        }
    }

    // Validate property names if propertyNames schema is present
    if (node.propertyNames)
    {
        for (auto it = obj.begin(); it != obj.end() && ctx.shouldContinue(); ++it)
        {
            const QString& propName{it.key()};
            validateNode(ctx,
                         ctx.schema.nodeAt(*node.propertyNames),
                         QJsonValue(propName),
                         instancePath,
                         schemaPath + u"/propertyNames"_qs);
        }
    }

    // Validate each property
    QSet<QString> evaluatedProperties{};

    for (auto it = obj.begin(); it != obj.end() && ctx.shouldContinue(); ++it)
    {
        const QString& propName{it.key()};
        const auto     propPath{json_pointer::appendToken(instancePath, propName)};
        bool           evaluated{false};

        // Check properties
        auto propIt{node.properties.find(propName)};
        if (propIt != node.properties.end())
        {
            validateNode(ctx,
                         ctx.schema.nodeAt(propIt->second),
                         it.value(),
                         propPath,
                         json_pointer::appendToken(schemaPath + u"/properties"_qs, propName));
            evaluated = true;
        }

        // Check patternProperties
        for (const auto& [pattern, schemaIndex] : node.patternProperties)
        {
            if (pattern.match(propName).hasMatch())
            {
                validateNode(
                    ctx, ctx.schema.nodeAt(schemaIndex), it.value(), propPath, schemaPath + u"/patternProperties"_qs);
                evaluated = true;
            }
        }

        // Check additionalProperties
        if (!evaluated && node.additionalProperties)
        {
            const auto& additionalNode{ctx.schema.nodeAt(*node.additionalProperties)};

            // If additionalProperties is false (BooleanSchema{false}), reject
            if (std::holds_alternative<BooleanSchema>(additionalNode))
            {
                if (!std::get<BooleanSchema>(additionalNode).value)
                {
                    const auto msg{QString(u"Additional property '%1' is not allowed").arg(propName)};
                    ctx.result.addError(propPath,
                                        schemaPath + u"/additionalProperties"_qs,
                                        msg,
                                        EvalError::AdditionalPropertiesInvalid);
                }
            }
            else
            {
                // Validate against additionalProperties schema
                validateNode(ctx, additionalNode, it.value(), propPath, schemaPath + u"/additionalProperties"_qs);
            }
        }

        if (evaluated)
            evaluatedProperties.insert(propName);
    }

    // Validate dependentRequired
    for (const auto& [propName, requiredProps] : node.dependentRequired)
    {
        if (obj.contains(propName))
        {
            for (const QString& requiredProp : requiredProps)
            {
                if (!obj.contains(requiredProp))
                {
                    const auto msg{QString(u"Property '%1' requires '%2' to be present")
                                       .arg(propName)
                                       .arg(requiredProp)};
                    ctx.result.addError(instancePath,
                                        schemaPath + u"/dependentRequired"_qs,
                                        msg,
                                        EvalError::RequiredMissing);
                }
            }
        }
    }

    // Validate dependentSchemas
    for (const auto& [propName, schemaIndex] : node.dependentSchemas)
    {
        if (obj.contains(propName))
        {
            validateNode(ctx,
                         ctx.schema.nodeAt(schemaIndex),
                         obj,
                         instancePath,
                         json_pointer::appendToken(schemaPath + u"/dependentSchemas"_qs, propName));
        }
    }
}

} // namespace json_query::json_schema::internal
