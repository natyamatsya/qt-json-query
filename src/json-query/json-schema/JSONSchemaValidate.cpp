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
#include "json-query/utils/QtStringLiterals.hpp"

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
    // Push resource dynamic anchors if this ObjectSchema is a resource root
    std::optional<DynamicScopeGuard> resourceGuard;
    if (node.selfIndex != ObjectSchema::kNoIndex)
    {
        const auto rdaIt{ctx.schema.resourceDynamicAnchors.find(node.selfIndex)};
        if (rdaIt != ctx.schema.resourceDynamicAnchors.end())
            resourceGuard.emplace(ctx.dynamicScope, rdaIt->second);
    }

    // Set up evaluation tracking if this schema uses unevaluatedProperties/unevaluatedItems.
    // Always create a local tracker so this schema only sees its own evaluations,
    // not evaluations from sibling keywords in a parent schema.
    const auto needsTracker{node.unevaluatedProperties || node.unevaluatedItems};
    EvaluationTracker  localTracker{};
    EvaluationTracker* savedTracker{ctx.tracker};
    if (needsTracker)
        ctx.tracker = &localTracker;

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

    // unevaluatedProperties / unevaluatedItems — must run after combinators have populated the tracker
    if (node.unevaluatedProperties && instance.isObject() && ctx.shouldContinue())
        validateUnevaluatedProperties(ctx, node, instance.toObject(), instancePath, schemaPath, validateNode);

    if (node.unevaluatedItems && instance.isArray() && ctx.shouldContinue())
        validateUnevaluatedItems(ctx, node, instance.toArray(), instancePath, schemaPath, validateNode);

    // Merge local evaluations into parent tracker and restore
    if (needsTracker)
    {
        if (savedTracker)
            savedTracker->mergeFrom(localTracker);
        ctx.tracker = savedTracker;
    }
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
    using json_query::literals::operator""_qt_s;

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
                                        u"Schema is false, all values are invalid"_qt_s,
                                        EvalError::ConstMismatch);
                }
                return;
            }

            if constexpr (std::is_same_v<T, ObjectSchema>)
            {
                validateObjectSchema(ctx, schemaVariant, instance, instancePath, schemaPath);
                return;
            }

            if constexpr (std::is_same_v<T, RefSchema>)
            {
                if (!schemaVariant.isResolved())
                    return; // Unresolved remote $ref — treat as true (accept all)

                const auto& rdaMap{ctx.schema.resourceDynamicAnchors};

                // Push this node's own resource scope (e.g., second_scope with $defs/$dynamicAnchor)
                std::optional<DynamicScopeGuard> selfGuard;
                if (schemaVariant.selfIndex != RefSchema::kNoIndex)
                {
                    const auto selfRda{rdaMap.find(schemaVariant.selfIndex)};
                    if (selfRda != rdaMap.end())
                        selfGuard.emplace(ctx.dynamicScope, selfRda->second);
                }

                // Push resource dynamic anchors if the target is a resource root
                const auto rdaIt{rdaMap.find(schemaVariant.targetIndex)};
                if (rdaIt != rdaMap.end())
                {
                    DynamicScopeGuard guard{ctx.dynamicScope, rdaIt->second};
                    validateNode(ctx, ctx.schema.nodeAt(schemaVariant.targetIndex), instance, instancePath, schemaPath);
                }
                else
                {
                    validateNode(ctx, ctx.schema.nodeAt(schemaVariant.targetIndex), instance, instancePath, schemaPath);
                }
                return;
            }

            if constexpr (std::is_same_v<T, DynamicRefSchema>)
            {
                if (!schemaVariant.isResolved())
                    return; // Unresolved — treat as true

                // Bookending requirement: only do dynamic resolution if
                // 1) the anchor name is a plain fragment (not a JSON Pointer starting with '/')
                // 2) the static target node itself has $dynamicAnchor with the same name
                auto targetIndex{schemaVariant.targetIndex};
                if (!schemaVariant.anchorName.isEmpty() && !schemaVariant.anchorName.startsWith(u'/'))
                {
                    const auto anchorIt{ctx.schema.nodeDynAnchorNames.find(targetIndex)};
                    if (anchorIt != ctx.schema.nodeDynAnchorNames.end() && anchorIt->second == schemaVariant.anchorName)
                    {
                        if (const auto resolved{ctx.resolveDynamicAnchor(schemaVariant.anchorName)})
                            targetIndex = *resolved;
                    }
                }

                // Push resource dynamic anchors if the target is a resource root
                const auto& rdaMap{ctx.schema.resourceDynamicAnchors};
                const auto  rdaIt{rdaMap.find(targetIndex)};
                if (rdaIt != rdaMap.end())
                {
                    DynamicScopeGuard guard{ctx.dynamicScope, rdaIt->second};
                    validateNode(ctx, ctx.schema.nodeAt(targetIndex), instance, instancePath, schemaPath);
                }
                else
                {
                    validateNode(ctx, ctx.schema.nodeAt(targetIndex), instance, instancePath, schemaPath);
                }
                return;
            }
        },
        node);
}

} // anonymous namespace

ValidationResult validateInstance(const internal::CompiledSchema& schema, const QJsonValue& instance)
{
    using json_query::literals::operator""_qt_s;

    ValidationResult result{};
    ValidateContext  ctx{schema, result, false};

    // Push root schema resource's dynamic anchors onto the scope
    const auto rootRda{schema.resourceDynamicAnchors.find(schema.rootIndex)};
    if (rootRda != schema.resourceDynamicAnchors.end())
        ctx.dynamicScope.push_back(DynamicScopeEntry{&rootRda->second});

    validateNode(ctx, schema.root(), instance, u""_qt_s, u"#"_qt_s);

    return result;
}

bool isInstanceValid(const internal::CompiledSchema& schema, const QJsonValue& instance)
{
    using json_query::literals::operator""_qt_s;

    ValidationResult result{};
    ValidateContext  ctx{schema, result, true}; // Stop on first error

    // Push root schema resource's dynamic anchors onto the scope
    const auto rootRda{schema.resourceDynamicAnchors.find(schema.rootIndex)};
    if (rootRda != schema.resourceDynamicAnchors.end())
        ctx.dynamicScope.push_back(DynamicScopeEntry{&rootRda->second});

    validateNode(ctx, schema.root(), instance, u""_qt_s, u"#"_qt_s);

    return result.isValid();
}

} // namespace json_query::json_schema
