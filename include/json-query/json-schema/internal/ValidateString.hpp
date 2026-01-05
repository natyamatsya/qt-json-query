// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/internal/FormatValidators.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"

#include <QString>

namespace json_query::json_schema::internal
{

/**
 * @brief Validate string constraints (minLength, maxLength, pattern, format)
 */
inline void validateString(ValidateContext&    ctx,
                           const ObjectSchema& node,
                           const QString&      str,
                           const QString&      instancePath,
                           const QString&      schemaPath)
{
    const auto length{static_cast<std::size_t>(str.length())};

    if (node.minLength && length < *node.minLength)
    {
        const auto msg{QString(u"String length %1 is less than minimum %2").arg(length).arg(*node.minLength)};
        ctx.result.addError(instancePath, schemaPath + u"/minLength"_qs, msg, EvalError::MinLengthViolation);
    }

    if (node.maxLength && length > *node.maxLength)
    {
        const auto msg{QString(u"String length %1 exceeds maximum %2").arg(length).arg(*node.maxLength)};
        ctx.result.addError(instancePath, schemaPath + u"/maxLength"_qs, msg, EvalError::MaxLengthViolation);
    }

    if (node.pattern && !node.pattern->match(str).hasMatch())
    {
        ctx.result.addError(instancePath,
                            schemaPath + u"/pattern"_qs,
                            u"String does not match required pattern"_qs,
                            EvalError::PatternMismatch);
    }

    if (node.format && !validateFormat(*node.format, str))
    {
        const auto msg{QString(u"String does not match format '%1'").arg(*node.format)};
        ctx.result.addError(instancePath, schemaPath + u"/format"_qs, msg, EvalError::FormatInvalid);
    }
}

} // namespace json_query::json_schema::internal
