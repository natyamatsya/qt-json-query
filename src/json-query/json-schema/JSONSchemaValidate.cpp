// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-schema/JSONSchemaValidate.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/ValidationHelpers.hpp"
#include "json-query/json-schema/internal/ValidateType.hpp"
#include "json-query/json-schema/internal/ValidateString.hpp"
#include "json-query/json-schema/internal/ValidateNumeric.hpp"
#include "json-query/json-schema/internal/ValidateArray.hpp"
#include "json-query/json-schema/internal/ValidateObject.hpp"
#include "json-query/json-schema/internal/ValidateCombinators.hpp"

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

// All validators now in modular headers with callback pattern

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

    // Type-specific validation (pass validateNode as callback for recursive validation)
    if (instance.isString() && ctx.shouldContinue())
        validateString(ctx, node, instance.toString(), instancePath, schemaPath);
    else if (instance.isDouble() && ctx.shouldContinue())
        validateNumeric(ctx, node, instance.toDouble(), instancePath, schemaPath);
    else if (instance.isArray() && ctx.shouldContinue())
        validateArray(ctx, node, instance.toArray(), instancePath, schemaPath, validateNode);
    else if (instance.isObject() && ctx.shouldContinue())
        validateObject(ctx, node, instance.toObject(), instancePath, schemaPath, validateNode);

    // Combinators
    if (ctx.shouldContinue())
        validateCombinators(ctx, node, instance, instancePath, schemaPath, validateNode);
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
