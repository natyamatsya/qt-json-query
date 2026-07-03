// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/internal/FormatValidators.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

#include <QString>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_schema::internal
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
    using json_query::literals::operator""_qt_s;

    const auto length{static_cast<std::size_t>(str.toUcs4().size())};

    if (node.minLength && length < *node.minLength)
    {
        const auto msg{QString(u"String length %1 is less than minimum %2").arg(length).arg(*node.minLength)};
        ctx.result.addError(instancePath, schemaPath + u"/minLength"_qt_s, msg, EvalError::MinLengthViolation);
    }

    if (node.maxLength && length > *node.maxLength)
    {
        const auto msg{QString(u"String length %1 exceeds maximum %2").arg(length).arg(*node.maxLength)};
        ctx.result.addError(instancePath, schemaPath + u"/maxLength"_qt_s, msg, EvalError::MaxLengthViolation);
    }

    if (node.pattern && !node.pattern->hasMatch(str))
    {
        ctx.result.addError(instancePath,
                            schemaPath + u"/pattern"_qt_s,
                            u"String does not match required pattern"_qt_s,
                            EvalError::PatternMismatch);
    }

    if (node.format && ctx.schema.formatAssertionEnabled && !validateFormat(*node.format, str))
    {
        const auto msg{QString(u"String does not match format '%1'").arg(*node.format)};
        ctx.result.addError(instancePath, schemaPath + u"/format"_qt_s, msg, EvalError::FormatInvalid);
    }
}

} // namespace json_query::json_schema::internal
