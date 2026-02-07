// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/ValidationContext.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

#include <QString>
#include <cmath>

namespace json_query::json_schema::internal
{

/**
 * @brief Validate numeric constraints (minimum, maximum, multipleOf, etc.)
 */
inline void validateNumeric(ValidateContext&    ctx,
                            const ObjectSchema& node,
                            double              value,
                            const QString&      instancePath,
                            const QString&      schemaPath)
{
    using json_query::literals::operator""_qt_s;

    if (node.minimum && value < *node.minimum)
    {
        const auto msg{QString(u"Value %1 is less than minimum %2").arg(value).arg(*node.minimum)};
        ctx.result.addError(instancePath, schemaPath + u"/minimum"_qt_s, msg, EvalError::MinimumViolation);
    }

    if (node.maximum && value > *node.maximum)
    {
        const auto msg{QString(u"Value %1 exceeds maximum %2").arg(value).arg(*node.maximum)};
        ctx.result.addError(instancePath, schemaPath + u"/maximum"_qt_s, msg, EvalError::MaximumViolation);
    }

    if (node.exclusiveMinimum && value <= *node.exclusiveMinimum)
    {
        const auto msg{QString(u"Value %1 must be greater than %2").arg(value).arg(*node.exclusiveMinimum)};
        ctx.result.addError(
            instancePath, schemaPath + u"/exclusiveMinimum"_qt_s, msg, EvalError::ExclusiveMinimumViolation);
    }

    if (node.exclusiveMaximum && value >= *node.exclusiveMaximum)
    {
        const auto msg{QString(u"Value %1 must be less than %2").arg(value).arg(*node.exclusiveMaximum)};
        ctx.result.addError(
            instancePath, schemaPath + u"/exclusiveMaximum"_qt_s, msg, EvalError::ExclusiveMaximumViolation);
    }

    if (node.multipleOf)
    {
        const auto divisor{*node.multipleOf};
        const auto quotient{value / divisor};
        const auto rounded{std::round(quotient)};
        // Check if quotient is close to an integer (handles precision better than fmod)
        if (std::abs(quotient - rounded) > 1e-8)
        {
            const auto msg{QString(u"Value %1 is not a multiple of %2").arg(value).arg(divisor)};
            ctx.result.addError(instancePath, schemaPath + u"/multipleOf"_qt_s, msg, EvalError::MultipleOfViolation);
        }
    }
}

} // namespace json_query::json_schema::internal
