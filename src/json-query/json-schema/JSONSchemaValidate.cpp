// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-schema/JSONSchemaValidate.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/ValidationHelpers.hpp"
#include "json-query/json-schema/internal/ValidateType.hpp"
#include "json-query/json-schema/internal/ValidateString.hpp"
#include "json-query/json-schema/internal/ValidateNumeric.hpp"
#include "json-query/json-pointer/JSONPointerUtils.hpp"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QSet>

#include <cmath>
#include <variant>

namespace json_query::json_schema
{

namespace
{

using namespace internal;

// Forward declaration
void validateNode(ValidateContext&  ctx,
                  const SchemaNode& node,
                  const QJsonValue& instance,
                  const QString&    instancePath,
                  const QString&    schemaPath);

// Validators from modular headers (ValidateType.hpp, ValidateString.hpp, ValidateNumeric.hpp)

/**
 * @brief Validate array constraints
 */
void validateArray(ValidateContext&    ctx,
                   const ObjectSchema& node,
                   const QJsonArray&   arr,
                   const QString&      instancePath,
                   const QString&      schemaPath)
{
    const auto size{static_cast<std::size_t>(arr.size())};

    if (node.minItems && size < *node.minItems)
    {
        const auto msg{QString(u"Array has %1 items, minimum is %2").arg(size).arg(*node.minItems)};
        ctx.result.addError(instancePath, schemaPath + u"/minItems"_qs, msg, EvalError::MinItemsViolation);
    }

    if (node.maxItems && size > *node.maxItems)
    {
        const auto msg{QString(u"Array has %1 items, maximum is %2").arg(size).arg(*node.maxItems)};
        ctx.result.addError(instancePath, schemaPath + u"/maxItems"_qs, msg, EvalError::MaxItemsViolation);
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
                                        schemaPath + u"/uniqueItems"_qs,
                                        u"Array items are not unique"_qs,
                                        EvalError::UniqueItemsViolation);
                    break;
                }
            }
        }
    }

    // Validate prefixItems
    for (std::size_t i = 0; i < node.prefixItems.size() && static_cast<int>(i) < arr.size() && ctx.shouldContinue();
         ++i)
    {
        const auto itemPath{instancePath + u"/"_qs + QString::number(i)};
        const auto itemSchemaPath{schemaPath + u"/prefixItems/"_qs + QString::number(i)};
        validateNode(ctx, ctx.schema.nodeAt(node.prefixItems[i]), arr[static_cast<int>(i)], itemPath, itemSchemaPath);
    }

    // Validate items (for elements after prefixItems)
    if (node.items)
    {
        const auto startIndex{node.prefixItems.size()};
        for (int i = static_cast<int>(startIndex); i < arr.size() && ctx.shouldContinue(); ++i)
        {
            const auto itemPath{instancePath + u"/"_qs + QString::number(i)};
            validateNode(ctx, ctx.schema.nodeAt(*node.items), arr[i], itemPath, schemaPath + u"/items"_qs);
        }
    }

    // Validate contains
    if (node.contains)
    {
        bool found = false;
        for (int i = 0; i < arr.size(); ++i)
        {
            ValidationResult tempResult{};
            ValidateContext  tempCtx{ctx.schema, tempResult, true};
            validateNode(tempCtx,
                         ctx.schema.nodeAt(*node.contains),
                         arr[i],
                         instancePath + u"/"_qs + QString::number(i),
                         schemaPath + u"/contains"_qs);
            if (tempResult.isValid())
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            ctx.result.addError(instancePath,
                                schemaPath + u"/contains"_qs,
                                u"Array does not contain required item"_qs,
                                EvalError::ContainsViolation);
        }
    }
}

/**
 * @brief Validate object constraints
 */
void validateObject(ValidateContext&    ctx,
                    const ObjectSchema& node,
                    const QJsonObject&  obj,
                    const QString&      instancePath,
                    const QString&      schemaPath)
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
    QSet<QString> evaluatedProperties;

    for (auto it = obj.begin(); it != obj.end() && ctx.shouldContinue(); ++it)
    {
        const QString& propName = it.key();
        const auto propPath{json_pointer::appendToken(instancePath, propName)};
        bool           evaluated = false;

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
            const auto& additionalNode = ctx.schema.nodeAt(*node.additionalProperties);

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

/**
 * @brief Validate combinators (allOf, anyOf, oneOf, not)
 */
void validateCombinators(ValidateContext&    ctx,
                         const ObjectSchema& node,
                         const QJsonValue&   instance,
                         const QString&      instancePath,
                         const QString&      schemaPath)
{
    // allOf: must match all schemas
    if (!node.allOf.empty())
    {
        for (std::size_t i = 0; i < node.allOf.size() && ctx.shouldContinue(); ++i)
        {
            validateNode(ctx,
                         ctx.schema.nodeAt(node.allOf[i]),
                         instance,
                         instancePath,
                         schemaPath + u"/allOf/"_qs + QString::number(i));
        }
    }

    // anyOf: must match at least one schema
    if (!node.anyOf.empty())
    {
        bool anyValid = false;
        for (std::size_t i = 0; i < node.anyOf.size(); ++i)
        {
            ValidationResult tempResult{};
            ValidateContext  tempCtx{ctx.schema, tempResult, true};
            validateNode(tempCtx,
                         ctx.schema.nodeAt(node.anyOf[i]),
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
        int matchCount = 0;
        for (std::size_t i = 0; i < node.oneOf.size(); ++i)
        {
            ValidationResult tempResult{};
            ValidateContext  tempCtx{ctx.schema, tempResult, true};
            validateNode(tempCtx,
                         ctx.schema.nodeAt(node.oneOf[i]),
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
            QString msg = matchCount == 0 ? u"Value does not match any schema in oneOf"_qs
                                          : u"Value matches more than one schema in oneOf"_qs;
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

/**
 * @brief Validate an ObjectSchema node
 */
void validateObjectSchema(ValidateContext&    ctx,
                          const ObjectSchema& node,
                          const QJsonValue&   instance,
                          const QString&      instancePath,
                          const QString&      schemaPath)
{
    // Type constraint
    if (node.type && ctx.shouldContinue())
        validateType(ctx, *node.type, instance, instancePath, schemaPath);

    // Enum constraint
    if (node.enumValues && ctx.shouldContinue())
        validateEnum(ctx, *node.enumValues, instance, instancePath, schemaPath);

    // Const constraint
    if (node.constValue && ctx.shouldContinue())
        validateConst(ctx, *node.constValue, instance, instancePath, schemaPath);

    // Type-specific validation
    if (instance.isString() && ctx.shouldContinue())
        validateString(ctx, node, instance.toString(), instancePath, schemaPath);
    else if (instance.isDouble() && ctx.shouldContinue())
        validateNumeric(ctx, node, instance.toDouble(), instancePath, schemaPath);
    else if (instance.isArray() && ctx.shouldContinue())
        validateArray(ctx, node, instance.toArray(), instancePath, schemaPath);
    else if (instance.isObject() && ctx.shouldContinue())
        validateObject(ctx, node, instance.toObject(), instancePath, schemaPath);

    // Combinators
    if (ctx.shouldContinue())
        validateCombinators(ctx, node, instance, instancePath, schemaPath);
}

/**
 * @brief Validate a schema node against an instance
 */
void validateNode(ValidateContext&  ctx,
                  const SchemaNode& node,
                  const QJsonValue& instance,
                  const QString&    instancePath,
                  const QString&    schemaPath)
{
    std::visit(
        [&](const auto& schemaVariant)
        {
            using T = std::decay_t<decltype(schemaVariant)>;

            if constexpr (std::is_same_v<T, BooleanSchema>)
            {
                if (!schemaVariant.value)
                {
                    ctx.result.addError(instancePath,
                                        schemaPath,
                                        u"Schema is false, all values are invalid"_qs,
                                        EvalError::ConstMismatch);
                }
                // true schema accepts everything
            }
            else if constexpr (std::is_same_v<T, ObjectSchema>)
            {
                validateObjectSchema(ctx, schemaVariant, instance, instancePath, schemaPath);
            }
            else if constexpr (std::is_same_v<T, RefSchema>)
            {
                // Follow the reference
                validateNode(ctx, ctx.schema.nodeAt(schemaVariant.targetIndex), instance, instancePath, schemaPath);
            }
        },
        node);
}

} // anonymous namespace

ValidationResult validateInstance(const internal::CompiledSchema& schema, const QJsonValue& instance)
{
    ValidationResult result{};
    ValidateContext  ctx{schema, result, false};

    validateNode(ctx, schema.root(), instance, u""_qs, u"#"_qs);

    return result;
}

bool isInstanceValid(const internal::CompiledSchema& schema, const QJsonValue& instance)
{
    ValidationResult result{};
    ValidateContext  ctx{schema, result, true}; // Stop on first error

    validateNode(ctx, schema.root(), instance, u""_qs, u"#"_qs);

    return result.isValid();
}

} // namespace json_query::json_schema
