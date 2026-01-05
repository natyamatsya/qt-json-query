// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/json-pointer/JSONPointerUtils.hpp"

#include <QJsonObject>
#include <QString>

namespace json_query::json_schema::internal
{

// ────────────────────────────────────────────────────────────────────────────
// Object Validation - Modular Helpers
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief Validate min/max properties constraints
 */
inline void validatePropertyCount(ValidateContext&    ctx,
                                  const ObjectSchema& node,
                                  std::size_t         size,
                                  const QString&      instancePath,
                                  const QString&      schemaPath)
{
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
}

/**
 * @brief Validate required properties
 */
inline void validateRequired(ValidateContext&    ctx,
                             const ObjectSchema& node,
                             const QJsonObject&  obj,
                             const QString&      instancePath,
                             const QString&      schemaPath)
{
    for (const QString& req : node.required)
    {
        if (!obj.contains(req))
        {
            const auto msg{QString(u"Required property '%1' is missing").arg(req)};
            ctx.result.addError(instancePath, schemaPath + u"/required"_qs, msg, EvalError::RequiredMissing);
        }
    }
}

/**
 * @brief Validate property names against propertyNames schema
 */
inline void validatePropertyNames(ValidateContext&    ctx,
                                  const ObjectSchema& node,
                                  const QJsonObject&  obj,
                                  const QString&      instancePath,
                                  const QString&      schemaPath,
                                  ValidateNodeFn&     validateNode)
{
    if (!node.propertyNames)
        return;

    for (auto it = obj.begin(); it != obj.end() && ctx.shouldContinue(); ++it)
    {
        validateNode(ctx,
                     ctx.schema.nodeAt(*node.propertyNames),
                     QJsonValue(it.key()),
                     instancePath,
                     schemaPath + u"/propertyNames"_qs);
    }
}

/**
 * @brief Check if property matches additionalProperties=false
 */
inline void rejectAdditionalProperty(ValidateContext& ctx,
                                     const QString&   propName,
                                     const QString&   propPath,
                                     const QString&   schemaPath)
{
    const auto msg{QString(u"Additional property '%1' is not allowed").arg(propName)};
    ctx.result.addError(propPath, schemaPath + u"/additionalProperties"_qs, msg, EvalError::AdditionalPropertiesInvalid);
}

/**
 * @brief Validate a single property against all applicable schemas
 * @return true if property was evaluated by properties or patternProperties
 */
inline bool validateSingleProperty(ValidateContext&    ctx,
                                   const ObjectSchema& node,
                                   const QString&      propName,
                                   const QJsonValue&   propValue,
                                   const QString&      propPath,
                                   const QString&      schemaPath,
                                   ValidateNodeFn&     validateNode)
{
    bool evaluated{false};

    // Check properties
    if (auto propIt = node.properties.find(propName); propIt != node.properties.end())
    {
        validateNode(ctx,
                     ctx.schema.nodeAt(propIt->second),
                     propValue,
                     propPath,
                     json_pointer::appendToken(schemaPath + u"/properties"_qs, propName));
        evaluated = true;
    }

    // Check patternProperties
    for (const auto& [pattern, schemaIndex] : node.patternProperties)
    {
        if (pattern.match(propName).hasMatch())
        {
            validateNode(ctx, ctx.schema.nodeAt(schemaIndex), propValue, propPath, schemaPath + u"/patternProperties"_qs);
            evaluated = true;
        }
    }

    // Check additionalProperties (only if not evaluated)
    if (!evaluated && node.additionalProperties)
    {
        const auto& additionalNode{ctx.schema.nodeAt(*node.additionalProperties)};

        if (const auto* boolSchema = std::get_if<BooleanSchema>(&additionalNode))
        {
            if (!boolSchema->value)
                rejectAdditionalProperty(ctx, propName, propPath, schemaPath);
        }
        else
        {
            validateNode(ctx, additionalNode, propValue, propPath, schemaPath + u"/additionalProperties"_qs);
        }
    }

    return evaluated;
}

/**
 * @brief Validate dependentRequired constraints
 */
inline void validateDependentRequired(ValidateContext&    ctx,
                                      const ObjectSchema& node,
                                      const QJsonObject&  obj,
                                      const QString&      instancePath,
                                      const QString&      schemaPath)
{
    for (const auto& [propName, requiredProps] : node.dependentRequired)
    {
        if (!obj.contains(propName))
            continue;

        for (const QString& requiredProp : requiredProps)
        {
            if (!obj.contains(requiredProp))
            {
                const auto msg{QString(u"Property '%1' requires '%2' to be present").arg(propName, requiredProp)};
                ctx.result.addError(instancePath, schemaPath + u"/dependentRequired"_qs, msg, EvalError::RequiredMissing);
            }
        }
    }
}

/**
 * @brief Validate dependentSchemas constraints
 */
inline void validateDependentSchemas(ValidateContext&    ctx,
                                     const ObjectSchema& node,
                                     const QJsonObject&  obj,
                                     const QString&      instancePath,
                                     const QString&      schemaPath,
                                     ValidateNodeFn&     validateNode)
{
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

// ────────────────────────────────────────────────────────────────────────────
// Main Object Validator - Clean Pipeline
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief Validate object constraints (properties, required, additionalProperties, etc.)
 */
inline void validateObject(ValidateContext&    ctx,
                           const ObjectSchema& node,
                           const QJsonObject&  obj,
                           const QString&      instancePath,
                           const QString&      schemaPath,
                           ValidateNodeFn&     validateNode)
{
    // Phase 1: Size constraints
    validatePropertyCount(ctx, node, static_cast<std::size_t>(obj.size()), instancePath, schemaPath);

    // Phase 2: Required properties
    validateRequired(ctx, node, obj, instancePath, schemaPath);

    // Phase 3: Property names
    validatePropertyNames(ctx, node, obj, instancePath, schemaPath, validateNode);

    // Phase 4: Validate each property
    for (auto it = obj.begin(); it != obj.end() && ctx.shouldContinue(); ++it)
    {
        const auto propPath{json_pointer::appendToken(instancePath, it.key())};
        validateSingleProperty(ctx, node, it.key(), it.value(), propPath, schemaPath, validateNode);
    }

    // Phase 5: Dependent constraints
    validateDependentRequired(ctx, node, obj, instancePath, schemaPath);
    validateDependentSchemas(ctx, node, obj, instancePath, schemaPath, validateNode);
}

} // namespace json_query::json_schema::internal
