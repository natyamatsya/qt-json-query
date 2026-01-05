// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/internal/ValidationHelpers.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <QString>

namespace json_query::json_schema::internal
{

/**
 * @brief Validate type constraint
 */
inline void validateType(ValidateContext&      ctx,
                         const TypeConstraint& constraint,
                         const QJsonValue&     instance,
                         const QString&        instancePath,
                         const QString&        schemaPath)
{
    const auto actualType{jsonValueToSchemaType(instance)};

    if (!constraint.allows(actualType))
    {
        const auto msg{QString(u"Expected type %1 but got %2")
                           .arg(schemaTypeToString(constraint.allowedTypes.front()))
                           .arg(schemaTypeToString(actualType))};
        ctx.result.addError(instancePath, schemaPath + u"/type"_qs, msg, EvalError::TypeMismatch);
    }
}

/**
 * @brief Validate enum constraint
 */
inline void validateEnum(ValidateContext&  ctx,
                         const QJsonArray& enumValues,
                         const QJsonValue& instance,
                         const QString&    instancePath,
                         const QString&    schemaPath)
{
    for (const QJsonValue& allowed : enumValues)
    {
        if (jsonValuesEqual(instance, allowed))
            return; // Match found
    }

    ctx.result.addError(instancePath,
                        schemaPath + u"/enum"_qs,
                        u"Value is not one of the allowed enum values"_qs,
                        EvalError::EnumMismatch);
}

/**
 * @brief Validate const constraint
 */
inline void validateConst(ValidateContext&  ctx,
                          const QJsonValue& constValue,
                          const QJsonValue& instance,
                          const QString&    instancePath,
                          const QString&    schemaPath)
{
    if (!jsonValuesEqual(instance, constValue))
    {
        ctx.result.addError(
            instancePath, schemaPath + u"/const"_qs, u"Value does not match const"_qs, EvalError::ConstMismatch);
    }
}

} // namespace json_query::json_schema::internal
