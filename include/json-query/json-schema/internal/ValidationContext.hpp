// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/JSONSchemaValidate.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"

namespace json_query::json_schema::internal
{

/**
 * @brief Context for schema validation
 *
 * Maintains state during recursive validation traversal.
 */
struct ValidateContext
{
    const CompiledSchema& schema;
    ValidationResult&     result;
    bool                  stopOnFirstError{false};

    /**
     * @brief Check if validation should continue
     *
     * Returns false if stopOnFirstError is set and an error has been recorded.
     */
    [[nodiscard]] bool shouldContinue() const noexcept
    {
        return !stopOnFirstError || result.isValid();
    }
};

/**
 * @brief Forward declaration for recursive validation
 *
 * This function pointer type allows modular validators to call back
 * into the main validation dispatcher for recursive validation.
 */
using ValidateNodeFn = void (*)(ValidateContext&,
                                const SchemaNode&,
                                const QJsonValue&,
                                const QString&,
                                const QString&);

} // namespace json_query::json_schema::internal
