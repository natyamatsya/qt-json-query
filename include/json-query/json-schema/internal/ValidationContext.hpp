// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/JSONSchemaValidate.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"

#include <unordered_set>

namespace json_query::json_schema::internal
{

/**
 * @brief Tracks which properties/items have been "evaluated" for unevaluatedProperties/unevaluatedItems
 */
struct EvaluationTracker
{
    std::unordered_set<QString> properties{};
    std::unordered_set<int>     items{};

    void mergeFrom(const EvaluationTracker& other)
    {
        properties.insert(other.properties.begin(), other.properties.end());
        items.insert(other.items.begin(), other.items.end());
    }
};

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
    EvaluationTracker*    tracker{nullptr};

    /**
     * @brief Check if validation should continue
     *
     * Returns false if stopOnFirstError is set and an error has been recorded.
     */
    [[nodiscard]] bool shouldContinue() const noexcept { return !stopOnFirstError || result.isValid(); }
};

/**
 * @brief Callback type for recursive validation
 *
 * This function reference type allows modular validators to call back
 * into the main validation dispatcher for recursive validation,
 * breaking the circular dependency.
 */
using ValidateNodeFn = void(ValidateContext&, const SchemaNode&, const QJsonValue&, const QString&, const QString&);

} // namespace json_query::json_schema::internal
