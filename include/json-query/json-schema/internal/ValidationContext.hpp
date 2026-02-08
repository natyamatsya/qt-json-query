// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/JSONSchemaValidate.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"

#include <unordered_set>
#include <vector>

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
 * @brief An entry in the dynamic scope stack for $dynamicRef resolution
 *
 * Each entry represents a schema resource boundary that has $dynamicAnchor entries.
 * The dynamic scope is walked at validation time to resolve $dynamicRef.
 */
struct DynamicScopeEntry
{
    const std::unordered_map<QString, std::size_t>* dynamicAnchors{nullptr};
};

/**
 * @brief RAII guard that pushes/pops a dynamic scope entry
 */
struct DynamicScopeGuard
{
    std::vector<DynamicScopeEntry>& stack;

    explicit DynamicScopeGuard(std::vector<DynamicScopeEntry>&                 s,
                               const std::unordered_map<QString, std::size_t>& anchors)
        : stack{s}
    {
        stack.push_back(DynamicScopeEntry{&anchors});
    }
    ~DynamicScopeGuard() { stack.pop_back(); }

    DynamicScopeGuard(const DynamicScopeGuard&)            = delete;
    DynamicScopeGuard& operator=(const DynamicScopeGuard&) = delete;
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

    /// Dynamic scope stack for $dynamicRef resolution
    std::vector<DynamicScopeEntry> dynamicScope{};

    /**
     * @brief Check if validation should continue
     *
     * Returns false if stopOnFirstError is set and an error has been recorded.
     */
    [[nodiscard]] bool shouldContinue() const noexcept { return !stopOnFirstError || result.isValid(); }

    /**
     * @brief Resolve a $dynamicRef by walking the dynamic scope
     *
     * Finds the outermost schema in the dynamic scope that has a
     * $dynamicAnchor matching the given name. Returns the node index
     * of that anchor, or std::nullopt if not found.
     */
    [[nodiscard]] std::optional<std::size_t> resolveDynamicAnchor(const QString& anchorName) const
    {
        // Walk from outermost (front) to innermost (back)
        for (const auto& entry : dynamicScope)
        {
            if (!entry.dynamicAnchors)
                continue;
            if (const auto it{entry.dynamicAnchors->find(anchorName)}; it != entry.dynamicAnchors->end())
                return it->second;
        }
        return std::nullopt;
    }
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
