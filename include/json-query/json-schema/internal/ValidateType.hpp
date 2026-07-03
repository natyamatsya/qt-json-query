// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/internal/ValidationHelpers.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

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
    using json_query::literals::operator""_qt_s;

    const auto actualType{jsonValueToSchemaType(instance)};

    if (!constraint.allows(actualType))
    {
        const auto msg{QString(u"Expected type %1 but got %2")
                           .arg(schemaTypeToString(constraint.allowedTypes.front()))
                           .arg(schemaTypeToString(actualType))};
        ctx.result.addError(instancePath, schemaPath + u"/type"_qt_s, msg, EvalError::TypeMismatch);
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
    using json_query::literals::operator""_qt_s;

    for (const auto& allowed : enumValues)
        if (jsonValuesEqual(instance, allowed))
            return; // Match found

    ctx.result.addError(instancePath,
                        schemaPath + u"/enum"_qt_s,
                        u"Value is not one of the allowed enum values"_qt_s,
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
    using json_query::literals::operator""_qt_s;

    if (!jsonValuesEqual(instance, constValue))
    {
        ctx.result.addError(
            instancePath, schemaPath + u"/const"_qt_s, u"Value does not match const"_qt_s, EvalError::ConstMismatch);
    }
}

} // namespace json_query::json_schema::internal
