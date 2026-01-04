// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-schema/JSONSchemaValidate.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"

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

/**
 * @brief Context for validation
 */
struct ValidateContext
{
    const CompiledSchema& schema;
    ValidationResult&     result;
    bool                  stopOnFirstError = false;

    [[nodiscard]] bool shouldContinue() const { return !stopOnFirstError || result.isValid(); }
};

// Forward declaration
void validateNode(ValidateContext&   ctx,
                  const SchemaNode&  node,
                  const QJsonValue&  instance,
                  const QString&     instancePath,
                  const QString&     schemaPath);

/**
 * @brief Check if two QJsonValues are equal (for enum/const)
 */
[[nodiscard]] bool jsonValuesEqual(const QJsonValue& a, const QJsonValue& b)
{
    if (a.type() != b.type())
    {
        return false;
    }

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
        const QJsonArray arrA = a.toArray();
        const QJsonArray arrB = b.toArray();
        if (arrA.size() != arrB.size())
            return false;
        for (int i = 0; i < arrA.size(); ++i)
        {
            if (!jsonValuesEqual(arrA[i], arrB[i]))
                return false;
        }
        return true;
    }
    case QJsonValue::Object:
    {
        const QJsonObject objA = a.toObject();
        const QJsonObject objB = b.toObject();
        if (objA.size() != objB.size())
            return false;
        for (auto it = objA.begin(); it != objA.end(); ++it)
        {
            if (!objB.contains(it.key()) || !jsonValuesEqual(it.value(), objB[it.key()]))
                return false;
        }
        return true;
    }
    default:
        return false;
    }
}

/**
 * @brief Validate type constraint
 */
void validateType(ValidateContext&       ctx,
                  const TypeConstraint&  constraint,
                  const QJsonValue&      instance,
                  const QString&         instancePath,
                  const QString&         schemaPath)
{
    SchemaType actualType = jsonValueToSchemaType(instance);

    if (!constraint.allows(actualType))
    {
        QString msg = QString(u"Expected type %1 but got %2")
                          .arg(schemaTypeToString(constraint.allowedTypes.front()))
                          .arg(schemaTypeToString(actualType));
        ctx.result.addError(instancePath, schemaPath + u"/type"_qs, msg, EvalError::TypeMismatch);
    }
}

/**
 * @brief Validate enum constraint
 */
void validateEnum(ValidateContext&  ctx,
                  const QJsonArray& enumValues,
                  const QJsonValue& instance,
                  const QString&    instancePath,
                  const QString&    schemaPath)
{
    for (const QJsonValue& allowed : enumValues)
    {
        if (jsonValuesEqual(instance, allowed))
        {
            return; // Match found
        }
    }

    ctx.result.addError(instancePath, schemaPath + u"/enum"_qs, u"Value is not one of the allowed enum values"_qs,
                        EvalError::EnumMismatch);
}

/**
 * @brief Validate const constraint
 */
void validateConst(ValidateContext&  ctx,
                   const QJsonValue& constValue,
                   const QJsonValue& instance,
                   const QString&    instancePath,
                   const QString&    schemaPath)
{
    if (!jsonValuesEqual(instance, constValue))
    {
        ctx.result.addError(instancePath, schemaPath + u"/const"_qs, u"Value does not match const"_qs,
                            EvalError::ConstMismatch);
    }
}

/**
 * @brief Validate string constraints
 */
void validateString(ValidateContext&    ctx,
                    const ObjectSchema& node,
                    const QString&      str,
                    const QString&      instancePath,
                    const QString&      schemaPath)
{
    const auto length = static_cast<std::size_t>(str.length());

    if (node.minLength && length < *node.minLength)
    {
        QString msg = QString(u"String length %1 is less than minimum %2").arg(length).arg(*node.minLength);
        ctx.result.addError(instancePath, schemaPath + u"/minLength"_qs, msg, EvalError::MinLengthViolation);
    }

    if (node.maxLength && length > *node.maxLength)
    {
        QString msg = QString(u"String length %1 exceeds maximum %2").arg(length).arg(*node.maxLength);
        ctx.result.addError(instancePath, schemaPath + u"/maxLength"_qs, msg, EvalError::MaxLengthViolation);
    }

    if (node.pattern && !node.pattern->match(str).hasMatch())
    {
        ctx.result.addError(instancePath, schemaPath + u"/pattern"_qs, u"String does not match required pattern"_qs,
                            EvalError::PatternMismatch);
    }
}

/**
 * @brief Validate numeric constraints
 */
void validateNumber(ValidateContext&    ctx,
                    const ObjectSchema& node,
                    double              value,
                    const QString&      instancePath,
                    const QString&      schemaPath)
{
    if (node.minimum && value < *node.minimum)
    {
        QString msg = QString(u"Value %1 is less than minimum %2").arg(value).arg(*node.minimum);
        ctx.result.addError(instancePath, schemaPath + u"/minimum"_qs, msg, EvalError::MinimumViolation);
    }

    if (node.maximum && value > *node.maximum)
    {
        QString msg = QString(u"Value %1 exceeds maximum %2").arg(value).arg(*node.maximum);
        ctx.result.addError(instancePath, schemaPath + u"/maximum"_qs, msg, EvalError::MaximumViolation);
    }

    if (node.exclusiveMinimum && value <= *node.exclusiveMinimum)
    {
        QString msg = QString(u"Value %1 must be greater than %2").arg(value).arg(*node.exclusiveMinimum);
        ctx.result.addError(instancePath, schemaPath + u"/exclusiveMinimum"_qs, msg,
                            EvalError::ExclusiveMinimumViolation);
    }

    if (node.exclusiveMaximum && value >= *node.exclusiveMaximum)
    {
        QString msg = QString(u"Value %1 must be less than %2").arg(value).arg(*node.exclusiveMaximum);
        ctx.result.addError(instancePath, schemaPath + u"/exclusiveMaximum"_qs, msg,
                            EvalError::ExclusiveMaximumViolation);
    }

    if (node.multipleOf)
    {
        double remainder = std::fmod(value, *node.multipleOf);
        // Allow for floating point imprecision
        if (std::abs(remainder) > 1e-10 && std::abs(remainder - *node.multipleOf) > 1e-10)
        {
            QString msg = QString(u"Value %1 is not a multiple of %2").arg(value).arg(*node.multipleOf);
            ctx.result.addError(instancePath, schemaPath + u"/multipleOf"_qs, msg, EvalError::MultipleOfViolation);
        }
    }
}

/**
 * @brief Validate array constraints
 */
void validateArray(ValidateContext&    ctx,
                   const ObjectSchema& node,
                   const QJsonArray&   arr,
                   const QString&      instancePath,
                   const QString&      schemaPath)
{
    const auto size = static_cast<std::size_t>(arr.size());

    if (node.minItems && size < *node.minItems)
    {
        QString msg = QString(u"Array has %1 items, minimum is %2").arg(size).arg(*node.minItems);
        ctx.result.addError(instancePath, schemaPath + u"/minItems"_qs, msg, EvalError::MinItemsViolation);
    }

    if (node.maxItems && size > *node.maxItems)
    {
        QString msg = QString(u"Array has %1 items, maximum is %2").arg(size).arg(*node.maxItems);
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
                    ctx.result.addError(instancePath, schemaPath + u"/uniqueItems"_qs,
                                        u"Array items are not unique"_qs, EvalError::UniqueItemsViolation);
                    break;
                }
            }
        }
    }

    // Validate prefixItems
    for (std::size_t i = 0; i < node.prefixItems.size() && static_cast<int>(i) < arr.size() && ctx.shouldContinue();
         ++i)
    {
        QString itemPath = instancePath + u"/"_qs + QString::number(i);
        QString itemSchemaPath = schemaPath + u"/prefixItems/"_qs + QString::number(i);
        validateNode(ctx, ctx.schema.nodeAt(node.prefixItems[i]), arr[static_cast<int>(i)], itemPath, itemSchemaPath);
    }

    // Validate items (for elements after prefixItems)
    if (node.items)
    {
        const std::size_t startIndex = node.prefixItems.size();
        for (int i = static_cast<int>(startIndex); i < arr.size() && ctx.shouldContinue(); ++i)
        {
            QString itemPath = instancePath + u"/"_qs + QString::number(i);
            validateNode(ctx, ctx.schema.nodeAt(*node.items), arr[i], itemPath, schemaPath + u"/items"_qs);
        }
    }

    // Validate contains
    if (node.contains)
    {
        bool found = false;
        for (int i = 0; i < arr.size(); ++i)
        {
            ValidationResult tempResult;
            ValidateContext  tempCtx{ctx.schema, tempResult, true};
            validateNode(tempCtx, ctx.schema.nodeAt(*node.contains), arr[i], instancePath + u"/"_qs + QString::number(i),
                         schemaPath + u"/contains"_qs);
            if (tempResult.isValid())
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            ctx.result.addError(instancePath, schemaPath + u"/contains"_qs,
                                u"Array does not contain required item"_qs, EvalError::ContainsViolation);
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
    const auto size = static_cast<std::size_t>(obj.size());

    if (node.minProperties && size < *node.minProperties)
    {
        QString msg = QString(u"Object has %1 properties, minimum is %2").arg(size).arg(*node.minProperties);
        ctx.result.addError(instancePath, schemaPath + u"/minProperties"_qs, msg, EvalError::MinPropertiesViolation);
    }

    if (node.maxProperties && size > *node.maxProperties)
    {
        QString msg = QString(u"Object has %1 properties, maximum is %2").arg(size).arg(*node.maxProperties);
        ctx.result.addError(instancePath, schemaPath + u"/maxProperties"_qs, msg, EvalError::MaxPropertiesViolation);
    }

    // Check required properties
    for (const QString& req : node.required)
    {
        if (!obj.contains(req))
        {
            QString msg = QString(u"Required property '%1' is missing").arg(req);
            ctx.result.addError(instancePath, schemaPath + u"/required"_qs, msg, EvalError::RequiredMissing);
        }
    }

    // Validate each property
    QSet<QString> evaluatedProperties;

    for (auto it = obj.begin(); it != obj.end() && ctx.shouldContinue(); ++it)
    {
        const QString& propName  = it.key();
        QString        propPath  = instancePath + u"/"_qs + propName;
        bool           evaluated = false;

        // Check properties
        auto propIt = node.properties.find(propName);
        if (propIt != node.properties.end())
        {
            validateNode(ctx, ctx.schema.nodeAt(propIt->second), it.value(), propPath,
                         schemaPath + u"/properties/"_qs + propName);
            evaluated = true;
        }

        // Check patternProperties
        for (const auto& [pattern, schemaIndex] : node.patternProperties)
        {
            if (pattern.match(propName).hasMatch())
            {
                validateNode(ctx, ctx.schema.nodeAt(schemaIndex), it.value(), propPath,
                             schemaPath + u"/patternProperties"_qs);
                evaluated = true;
            }
        }

        // Check additionalProperties
        if (!evaluated && node.additionalProperties)
        {
            const SchemaNode& additionalNode = ctx.schema.nodeAt(*node.additionalProperties);

            // If additionalProperties is false (BooleanSchema{false}), reject
            if (std::holds_alternative<BooleanSchema>(additionalNode))
            {
                if (!std::get<BooleanSchema>(additionalNode).value)
                {
                    QString msg = QString(u"Additional property '%1' is not allowed").arg(propName);
                    ctx.result.addError(propPath, schemaPath + u"/additionalProperties"_qs, msg,
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
        {
            evaluatedProperties.insert(propName);
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
            validateNode(ctx, ctx.schema.nodeAt(node.allOf[i]), instance, instancePath,
                         schemaPath + u"/allOf/"_qs + QString::number(i));
        }
    }

    // anyOf: must match at least one schema
    if (!node.anyOf.empty())
    {
        bool anyValid = false;
        for (std::size_t i = 0; i < node.anyOf.size(); ++i)
        {
            ValidationResult tempResult;
            ValidateContext  tempCtx{ctx.schema, tempResult, true};
            validateNode(tempCtx, ctx.schema.nodeAt(node.anyOf[i]), instance, instancePath,
                         schemaPath + u"/anyOf/"_qs + QString::number(i));
            if (tempResult.isValid())
            {
                anyValid = true;
                break;
            }
        }
        if (!anyValid)
        {
            ctx.result.addError(instancePath, schemaPath + u"/anyOf"_qs,
                                u"Value does not match any schema in anyOf"_qs, EvalError::AnyOfFailed);
        }
    }

    // oneOf: must match exactly one schema
    if (!node.oneOf.empty())
    {
        int matchCount = 0;
        for (std::size_t i = 0; i < node.oneOf.size(); ++i)
        {
            ValidationResult tempResult;
            ValidateContext  tempCtx{ctx.schema, tempResult, true};
            validateNode(tempCtx, ctx.schema.nodeAt(node.oneOf[i]), instance, instancePath,
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
        ValidationResult tempResult;
        ValidateContext  tempCtx{ctx.schema, tempResult, true};
        validateNode(tempCtx, ctx.schema.nodeAt(*node.notSchema), instance, instancePath, schemaPath + u"/not"_qs);
        if (tempResult.isValid())
        {
            ctx.result.addError(instancePath, schemaPath + u"/not"_qs, u"Value matches schema in not"_qs,
                                EvalError::NotFailed);
        }
    }

    // if/then/else
    if (node.ifSchema)
    {
        ValidationResult ifResult;
        ValidateContext  ifCtx{ctx.schema, ifResult, true};
        validateNode(ifCtx, ctx.schema.nodeAt(*node.ifSchema), instance, instancePath, schemaPath + u"/if"_qs);

        if (ifResult.isValid())
        {
            if (node.thenSchema)
            {
                validateNode(ctx, ctx.schema.nodeAt(*node.thenSchema), instance, instancePath, schemaPath + u"/then"_qs);
            }
        }
        else
        {
            if (node.elseSchema)
            {
                validateNode(ctx, ctx.schema.nodeAt(*node.elseSchema), instance, instancePath, schemaPath + u"/else"_qs);
            }
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
    {
        validateType(ctx, *node.type, instance, instancePath, schemaPath);
    }

    // Enum constraint
    if (node.enumValues && ctx.shouldContinue())
    {
        validateEnum(ctx, *node.enumValues, instance, instancePath, schemaPath);
    }

    // Const constraint
    if (node.constValue && ctx.shouldContinue())
    {
        validateConst(ctx, *node.constValue, instance, instancePath, schemaPath);
    }

    // Type-specific validation
    if (instance.isString() && ctx.shouldContinue())
    {
        validateString(ctx, node, instance.toString(), instancePath, schemaPath);
    }
    else if (instance.isDouble() && ctx.shouldContinue())
    {
        validateNumber(ctx, node, instance.toDouble(), instancePath, schemaPath);
    }
    else if (instance.isArray() && ctx.shouldContinue())
    {
        validateArray(ctx, node, instance.toArray(), instancePath, schemaPath);
    }
    else if (instance.isObject() && ctx.shouldContinue())
    {
        validateObject(ctx, node, instance.toObject(), instancePath, schemaPath);
    }

    // Combinators
    if (ctx.shouldContinue())
    {
        validateCombinators(ctx, node, instance, instancePath, schemaPath);
    }
}

/**
 * @brief Validate a schema node against an instance
 */
void validateNode(ValidateContext&   ctx,
                  const SchemaNode&  node,
                  const QJsonValue&  instance,
                  const QString&     instancePath,
                  const QString&     schemaPath)
{
    std::visit(
        [&](const auto& schemaVariant)
        {
            using T = std::decay_t<decltype(schemaVariant)>;

            if constexpr (std::is_same_v<T, BooleanSchema>)
            {
                if (!schemaVariant.value)
                {
                    ctx.result.addError(instancePath, schemaPath, u"Schema is false, all values are invalid"_qs,
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
    ValidationResult result;
    ValidateContext  ctx{schema, result, false};

    validateNode(ctx, schema.root(), instance, u""_qs, u"#"_qs);

    return result;
}

bool isInstanceValid(const internal::CompiledSchema& schema, const QJsonValue& instance)
{
    ValidationResult result;
    ValidateContext  ctx{schema, result, true}; // Stop on first error

    validateNode(ctx, schema.root(), instance, u""_qs, u"#"_qs);

    return result.isValid();
}

} // namespace json_query::json_schema
