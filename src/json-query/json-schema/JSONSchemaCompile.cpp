// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-schema/JSONSchemaCompile.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/internal/CompileContext.hpp"
#include "json-query/json-schema/internal/CompileKeywords.hpp"
#include "json-query/json-schema/internal/CompileDispatch.hpp"
#include "json-query/json-pointer/JSONPointerParsing.hpp"
#include "json-query/json-pointer/JSONPointerEvaluation.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>
#include <QtCore/QDebug>

#include <array>
#include <ranges>

namespace json_query::json_schema
{

namespace
{

using namespace internal;

// Forward declaration for recursive compilation
std::expected<std::size_t, QueryError> compileSchemaNode(CompileContext& ctx, const QJsonValue& schemaValue);

/**
 * @brief Compile properties keyword
 */
std::expected<std::unordered_map<QString, std::size_t>, QueryError> compileProperties(CompileContext&   ctx,
                                                                                      const QJsonValue& propsValue)
{
    std::unordered_map<QString, std::size_t> result;

    if (propsValue.isUndefined())
        return result;

    if (!propsValue.isObject())
        return std::unexpected(QueryError(ParseError::InvalidKeywordValue));

    const auto propsObj{propsValue.toObject()};
    for (auto it = propsObj.begin(); it != propsObj.end(); ++it)
    {
        auto nodeIndex{compileSchemaNode(ctx, it.value())};
        if (!nodeIndex)
            return std::unexpected(nodeIndex.error());
        result[it.key()] = *nodeIndex;
    }

    return result;
}

/**
 * @brief Compile array of schemas (for allOf, anyOf, oneOf)
 */
std::expected<std::vector<std::size_t>, QueryError> compileSchemaArray(CompileContext&   ctx,
                                                                       const QJsonValue& arrayValue)
{
    std::vector<std::size_t> result{};

    if (arrayValue.isUndefined())
        return result;

    if (!arrayValue.isArray())
        return std::unexpected(QueryError(ParseError::InvalidKeywordValue));

    const auto arr{asArray(arrayValue)};
    result.reserve(static_cast<std::size_t>(arr.size()));

    for (const QJsonValue& item : arr)
    {
        auto nodeIndex{compileSchemaNode(ctx, item)};
        if (!nodeIndex)
            return std::unexpected(nodeIndex.error());
        result.push_back(*nodeIndex);
    }

    return result;
}

/**
 * @brief Compile an optional sub-schema
 */
std::expected<std::optional<std::size_t>, QueryError> compileOptionalSchema(CompileContext&   ctx,
                                                                            const QJsonValue& schemaValue)
{
    if (schemaValue.isUndefined())
        return std::nullopt;

    auto nodeIndex{compileSchemaNode(ctx, schemaValue)};
    if (!nodeIndex)
        return std::unexpected(nodeIndex.error());
    return *nodeIndex;
}

// ────────────────────────────────────────────────────────────────────────────
// Schema Node Compilation - Modular Helpers
// ────────────────────────────────────────────────────────────────────────────

/// Metadata-only keywords that don't affect validation
static constexpr std::array kMetadataOnlyKeys{
    u"$ref",         u"$anchor",        u"$id",          u"$schema",
    u"$defs",        u"definitions",    u"title",        u"description",
    u"$comment",     u"$dynamicRef",    u"$dynamicAnchor"};

/**
 * @brief Check if schema contains validation keywords beyond metadata
 */
[[nodiscard]] bool hasValidationKeywords(const QJsonObject& schemaObj)
{
    return std::ranges::any_of(
        schemaObj.keys(),
        [](const QString& key)
        { return std::ranges::none_of(kMetadataOnlyKeys, [&](const char16_t* mk) { return key == mk; }); });
}

/**
 * @brief Extract $id and update base URI if present
 */
void processSchemaId(CompileContext& ctx, const QJsonObject& schemaObj, const QJsonValue& schemaValue)
{
    using json_query::literals::operator""_qt_s;

    if (schemaObj.contains(u"$id"_qt_s) && schemaObj[u"$id"_qt_s].isString())
    {
        ctx.baseUri = schemaObj[u"$id"_qt_s].toString();
        ctx.resourceDocuments[ctx.baseUri] = schemaValue;
    }
}

/**
 * @brief Extract $anchor name if present
 */
[[nodiscard]] std::optional<QString> extractAnchorName(const QJsonObject& schemaObj)
{
    using json_query::literals::operator""_qt_s;

    if (schemaObj.contains(u"$anchor"_qt_s) && schemaObj[u"$anchor"_qt_s].isString())
        return schemaObj[u"$anchor"_qt_s].toString();
    return std::nullopt;
}

/**
 * @brief Extract $dynamicAnchor name if present
 */
[[nodiscard]] std::optional<QString> extractDynamicAnchorName(const QJsonObject& schemaObj)
{
    using json_query::literals::operator""_qt_s;

    if (schemaObj.contains(u"$dynamicAnchor"_qt_s) && schemaObj[u"$dynamicAnchor"_qt_s].isString())
        return schemaObj[u"$dynamicAnchor"_qt_s].toString();
    return std::nullopt;
}

/**
 * @brief Register anchor in context, checking for duplicates
 */
[[nodiscard]] std::expected<void, QueryError>
registerAnchor(CompileContext& ctx, const QString& anchorName, std::size_t nodeIndex)
{
    const auto scopedKey{ctx.scopedAnchorKey(anchorName)};
    ctx.anchors[scopedKey] = nodeIndex;
    return {};
}

/**
 * @brief RAII guard for base URI scope management
 */
struct BaseUriScope
{
    CompileContext& ctx;
    QString         saved;

    explicit BaseUriScope(CompileContext& c) : ctx{c}, saved{c.baseUri} {}
    ~BaseUriScope() { ctx.baseUri = saved; }
};

/**
 * @brief Compile a single schema node (recursive worker function)
 *
 * This is the core recursive compilation function used throughout all phases.
 * It compiles a single schema (object or boolean) into a node and registers
 * any $anchor it contains.
 *
 * Called by:
 * - Phase 1: phase1_BuildSymbolTable() to compile definition schemas
 * - Phase 2: compileSchema() to compile the root schema
 * - Recursively: By itself via the keyword dispatcher for nested schemas
 *
 * The function handles:
 * - Boolean schemas (true/false)
 * - $ref-only schemas (fast path)
 * - Full object schemas with validation keywords
 * - $anchor registration
 * - Base URI scope management
 */
std::expected<std::size_t, QueryError> compileSchemaNode(CompileContext& ctx, const QJsonValue& schemaValue)
{
    using json_query::literals::operator""_qt_s;

    // Fast path: Boolean schema
    if (schemaValue.isBool())
        return ctx.addNode(BooleanSchema{schemaValue.toBool()});

    if (!schemaValue.isObject())
        return std::unexpected(QueryError(ParseError::InvalidSchemaStructure));

    const auto schemaObj{schemaValue.toObject()};

    // Phase 1: Scope management
    BaseUriScope uriScope{ctx};
    processSchemaId(ctx, schemaObj, schemaValue);
    const auto anchorName{extractAnchorName(schemaObj)};
    const auto dynAnchorName{extractDynamicAnchorName(schemaObj)};

    // Helper: qualify local fragment refs with the current base URI
    // so "#items" inside resource "list" becomes "list#items"
    // and "#/$defs/items" inside "list" becomes "list#/$defs/items"
    auto qualifyRef = [&](const QString& ref) -> QString
    {
        if (ref.startsWith(u'#') && ref.size() > 1 && !ctx.baseUri.isEmpty())
            return ctx.baseUri + ref;
        return ref;
    };

    // Phase 2: Fast path for $ref-only or $dynamicRef-only schemas
    const auto hasRef{schemaObj.contains(u"$ref"_qt_s)};
    const auto hasDynRef{schemaObj.contains(u"$dynamicRef"_qt_s)};

    // Helper: register $id and set selfIndex for fast-path nodes
    auto registerIdForFastPath = [&](std::size_t nodeIndex)
    {
        if (schemaObj.contains(u"$id"_qt_s) && schemaObj[u"$id"_qt_s].isString())
            ctx.anchors[schemaObj[u"$id"_qt_s].toString()] = nodeIndex;
        // Set selfIndex for resource scope management
        if (auto* ref{std::get_if<RefSchema>(&ctx.nodes[nodeIndex])})
            ref->selfIndex = nodeIndex;
        else if (auto* dynRef{std::get_if<DynamicRefSchema>(&ctx.nodes[nodeIndex])})
            dynRef->selfIndex = nodeIndex;
        return nodeIndex;
    };

    if (hasRef && !hasDynRef && !hasValidationKeywords(schemaObj))
        return registerIdForFastPath(ctx.addNode(RefSchema{RefSchema::kUnresolved, qualifyRef(schemaObj[u"$ref"_qt_s].toString())}));

    if (hasDynRef && !hasRef && !hasValidationKeywords(schemaObj))
    {
        const auto dynRefStr{qualifyRef(schemaObj[u"$dynamicRef"_qt_s].toString())};
        auto fragment{dynRefStr};
        if (const auto hashPos{dynRefStr.indexOf(u'#')}; hashPos >= 0)
            fragment = dynRefStr.mid(hashPos + 1);
        return registerIdForFastPath(ctx.addNode(DynamicRefSchema{DynamicRefSchema::kUnresolved, fragment, DynamicRefSchema::kUnresolved, dynRefStr}));
    }

    // Phase 3: Build ObjectSchema via dispatch
    ObjectSchema node{};

    if (hasRef)
        node.allOf.push_back(ctx.addNode(RefSchema{RefSchema::kUnresolved, qualifyRef(schemaObj[u"$ref"_qt_s].toString())}));

    if (hasDynRef)
    {
        const auto dynRefStr{qualifyRef(schemaObj[u"$dynamicRef"_qt_s].toString())};
        auto fragment{dynRefStr};
        if (const auto hashPos{dynRefStr.indexOf(u'#')}; hashPos >= 0)
            fragment = dynRefStr.mid(hashPos + 1);
        node.allOf.push_back(ctx.addNode(DynamicRefSchema{DynamicRefSchema::kUnresolved, fragment, DynamicRefSchema::kUnresolved, dynRefStr}));
    }

    if (auto r{FullKeywordDispatcher::dispatch(ctx, schemaObj, node, compileSchemaNode)}; !r)
        return std::unexpected(r.error());

    // Phase 4: Register node and anchor
    const auto nodeIndex{ctx.addNode(std::move(node))};

    // Store self-index for resource scope management during validation
    if (auto* obj{std::get_if<ObjectSchema>(&ctx.nodes[nodeIndex])})
        obj->selfIndex = nodeIndex;

    if (anchorName)
    {
        if (auto r{registerAnchor(ctx, *anchorName, nodeIndex)}; !r)
            return std::unexpected(r.error());
    }

    // Register $dynamicAnchor: stored in both anchors (for static $ref fallback) and dynamicAnchors
    if (dynAnchorName)
    {
        const auto scopedKey{ctx.scopedAnchorKey(*dynAnchorName)};
        ctx.anchors[scopedKey] = nodeIndex;
        ctx.dynamicAnchors[scopedKey] = nodeIndex;
        // Also register unscoped for simple fragment lookup
        ctx.anchors[*dynAnchorName] = nodeIndex;
        ctx.dynamicAnchors[*dynAnchorName] = nodeIndex;
        // Record for per-resource map building
        ctx.pendingDynamicAnchors.push_back({ctx.baseUri, *dynAnchorName, nodeIndex});
    }

    // Register $id URI so $ref can resolve against it
    if (schemaObj.contains(u"$id"_qt_s) && schemaObj[u"$id"_qt_s].isString())
    {
        const auto id{schemaObj[u"$id"_qt_s].toString()};
        ctx.anchors[id] = nodeIndex;
    }

    return nodeIndex;
}

// ────────────────────────────────────────────────────────────────────────────
// Schema Compilation Pipeline - Modular Helpers
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief Extract root schema metadata ($id, $schema)
 */
void extractRootMetadata(const QJsonObject& rootObj, internal::CompiledSchema& compiled)
{
    using json_query::literals::operator""_qt_s;

    if (rootObj.contains(u"$id"_qt_s) && rootObj[u"$id"_qt_s].isString())
        compiled.schemaId = rootObj[u"$id"_qt_s].toString();

    if (rootObj.contains(u"$schema"_qt_s) && rootObj[u"$schema"_qt_s].isString())
        compiled.dialect = rootObj[u"$schema"_qt_s].toString();
}

/**
 * @brief Compile definitions block ($defs or definitions)
 */
[[nodiscard]] std::expected<void, QueryError> compileDefinitionsBlock(CompileContext&    ctx,
                                                                      const QJsonObject& rootObj,
                                                                      const QString&     keyword,
                                                                      const QString&     pathPrefix)
{
    using json_query::literals::operator""_qt_s;

    if (!rootObj.contains(keyword))
        return {};

    const auto defsValue{rootObj[keyword]};
    if (!defsValue.isObject())
        return {};

    const auto defsObj{defsValue.toObject()};
    for (auto it = defsObj.begin(); it != defsObj.end(); ++it)
    {
        auto defResult{compileSchemaNode(ctx, it.value())};
        if (!defResult)
            return std::unexpected(defResult.error());

        ctx.anchors[pathPrefix + it.key()] = *defResult;

        // Recursively compile nested $defs within this definition
        // This ensures $dynamicAnchor entries in nested definitions get registered
        if (it.value().isObject())
        {
            const auto defObj{it.value().toObject()};
            const auto nestedPrefix{pathPrefix + it.key() + u"/"_qt_s};

            // Restore the definition's $id as baseUri for correct $dynamicAnchor scoping
            // (compileSchemaNode's BaseUriScope already restored baseUri when it returned)
            BaseUriScope nestedScope{ctx};
            if (defObj.contains(u"$id"_qt_s) && defObj[u"$id"_qt_s].isString())
                ctx.baseUri = defObj[u"$id"_qt_s].toString();

            if (auto r{compileDefinitionsBlock(ctx, defObj, u"$defs"_qt_s, nestedPrefix + u"$defs/"_qt_s)}; !r)
                return std::unexpected(r.error());
            if (auto r{compileDefinitionsBlock(ctx, defObj, u"definitions"_qt_s, nestedPrefix + u"definitions/"_qt_s)}; !r)
                return std::unexpected(r.error());
        }
    }

    return {};
}

/**
 * @brief Recursively scan a JSON value for $defs blocks buried in sub-schemas
 *
 * Phase 1 normally only compiles root-level $defs. This function scans the
 * entire schema tree to find $defs in inline schemas (if/then/else, properties, etc.)
 * so that $dynamicAnchor entries are registered before Phase 2.
 *
 * Skips $defs and definitions keys themselves (already handled by compileDefinitionsBlock).
 */
void scanDefsInSchemaTree(CompileContext& ctx, const QJsonValue& value)
{
    using json_query::literals::operator""_qt_s;

    if (!value.isObject())
        return;

    const auto obj{value.toObject()};

    for (auto it{obj.begin()}; it != obj.end(); ++it)
    {
        const auto& key{it.key()};

        // Skip $defs/definitions — handled by compileDefinitionsBlock at each level
        if (key == u"$defs"_qt_s || key == u"definitions"_qt_s)
            continue;

        if (it.value().isObject())
        {
            const auto childObj{it.value().toObject()};

            // If this sub-schema has $defs, compile them
            BaseUriScope scope{ctx};
            if (childObj.contains(u"$id"_qt_s) && childObj[u"$id"_qt_s].isString())
                ctx.baseUri = childObj[u"$id"_qt_s].toString();

            if (auto r{compileDefinitionsBlock(ctx, childObj, u"$defs"_qt_s, u""_qt_s)}; !r)
                return; // Best-effort; errors handled in Phase 2
            if (auto r{compileDefinitionsBlock(ctx, childObj, u"definitions"_qt_s, u""_qt_s)}; !r)
                return;

            // Recurse deeper
            scanDefsInSchemaTree(ctx, it.value());
        }
        else if (it.value().isArray())
        {
            for (const auto& item : it.value().toArray())
                scanDefsInSchemaTree(ctx, item);
        }
    }
}

/**
 * @brief Percent-decode a URI fragment (e.g., %22 → ", %25 → %)
 */
QString percentDecode(const QString& input)
{
    QString result{};
    result.reserve(input.size());
    for (int i{0}; i < input.size(); ++i)
    {
        if (input[i] == u'%' && i + 2 < input.size())
        {
            const auto hex{input.mid(i + 1, 2)};
            bool ok{false};
            const auto ch{static_cast<char>(hex.toInt(&ok, 16))};
            if (ok)
            {
                result.append(QChar::fromLatin1(ch));
                i += 2;
                continue;
            }
        }
        result.append(input[i]);
    }
    return result;
}

/**
 * @brief Resolve a single $ref reference
 */
void resolveReference(RefSchema&                                      refNode,
                      std::size_t                                     rootIndex,
                      const std::unordered_map<QString, std::size_t>& anchors,
                      const QJsonValue&                               rootSchema,
                      CompileContext&                                  ctx)
{
    using json_query::literals::operator""_qt_s;

    const auto& ref{refNode.originalRef};

    // Root reference
    if (ref == u"#"_qt_s)
    {
        refNode.targetIndex = rootIndex;
        return;
    }

    // Direct anchor match
    if (anchors.contains(ref))
    {
        refNode.targetIndex = anchors.at(ref);
        return;
    }

    // Local fragment reference (e.g., "#foo" or "#/properties/foo")
    if (ref.startsWith(u"#"_qt_s))
    {
        const auto fragment{ref.mid(1)};

        // Try anchor lookup first
        if (anchors.contains(fragment))
        {
            refNode.targetIndex = anchors.at(fragment);
            return;
        }

        // Try JSON Pointer resolution against the root schema document
        if (fragment.startsWith(u"/"_qt_s))
        {
            const auto decoded{percentDecode(fragment)};
            std::vector<json_pointer::Token> tokens{};
            if (json_pointer::detail::parsePointer(decoded, tokens))
            {
                const auto resolved{json_pointer::detail::evaluatePointerImpl(tokens, rootSchema)};
                if (resolved)
                {
                    auto compiled{compileSchemaNode(ctx, *resolved)};
                    if (compiled)
                    {
                        refNode.targetIndex = *compiled;
                        return;
                    }
                }
            }
        }
        return;
    }

    // URI with fragment (e.g., "http://example.com/nested.json#foo" or "list#/$defs/items")
    if (ref.contains(u'#'))
    {
        const auto hashPos{ref.lastIndexOf(u'#')};
        const auto baseUri{ref.left(hashPos)};
        const auto fragment{ref.mid(hashPos + 1)};

        // Try anchor lookup for the fragment
        if (anchors.contains(fragment))
        {
            refNode.targetIndex = anchors.at(fragment);
            return;
        }

        // Try JSON Pointer resolution against a sub-resource document
        if (fragment.startsWith(u"/"_qt_s))
        {
            const auto docIt{ctx.resourceDocuments.find(baseUri)};
            if (docIt != ctx.resourceDocuments.end())
            {
                const auto decoded{percentDecode(fragment)};
                std::vector<json_pointer::Token> tokens{};
                if (json_pointer::detail::parsePointer(decoded, tokens))
                {
                    const auto resolved{json_pointer::detail::evaluatePointerImpl(tokens, docIt->second)};
                    if (resolved)
                    {
                        // If the resolved schema has $id, reuse the existing compiled node
                        // to preserve resource dynamic anchor mappings
                        if (resolved->isObject())
                        {
                            const auto resolvedObj{resolved->toObject()};
                            if (resolvedObj.contains(u"$id"_qt_s) && resolvedObj[u"$id"_qt_s].isString())
                            {
                                const auto resolvedId{resolvedObj[u"$id"_qt_s].toString()};
                                if (anchors.contains(resolvedId))
                                {
                                    refNode.targetIndex = anchors.at(resolvedId);
                                    return;
                                }
                            }
                        }

                        // Set baseUri to containing resource for correct ref qualification
                        BaseUriScope resolutionScope{ctx};
                        ctx.baseUri = baseUri;

                        auto compiled{compileSchemaNode(ctx, *resolved)};
                        if (compiled)
                        {
                            refNode.targetIndex = *compiled;

                            // Set selfIndex to containing resource root so its scope is pushed
                            if (anchors.contains(baseUri))
                            {
                                const auto resourceRootIdx{anchors.at(baseUri)};
                                if (auto* newRef{std::get_if<RefSchema>(&ctx.nodes[*compiled])})
                                    newRef->selfIndex = resourceRootIdx;
                                else if (auto* newDynRef{std::get_if<DynamicRefSchema>(&ctx.nodes[*compiled])})
                                    newDynRef->selfIndex = resourceRootIdx;
                            }
                            return;
                        }
                    }
                }
            }
        }

        // Extract filename and build scoped key
        const auto lastSlash{baseUri.lastIndexOf(u'/')};
        const auto filename{lastSlash >= 0 ? baseUri.mid(lastSlash + 1) : baseUri};
        const auto scopedKey{filename + u"#"_qt_s + fragment};

        if (anchors.contains(scopedKey))
            refNode.targetIndex = anchors.at(scopedKey);
    }
}

/**
 * @brief Resolve a DynamicRefSchema's static fallback target
 */
void resolveDynamicReference(DynamicRefSchema&                                   dynRef,
                             std::size_t                                         rootIndex,
                             const std::unordered_map<QString, std::size_t>&     anchors,
                             const QJsonValue&                                   rootSchema,
                             CompileContext&                                      ctx)
{
    using json_query::literals::operator""_qt_s;

    if (dynRef.isResolved())
        return;

    // If the original ref looks like a normal $ref (e.g. "#/$defs/items" or "#foo"),
    // resolve it as a RefSchema would, then copy the target index back
    RefSchema tempRef{RefSchema::kUnresolved, dynRef.originalRef};
    resolveReference(tempRef, rootIndex, anchors, rootSchema, ctx);
    if (tempRef.isResolved())
    {
        dynRef.targetIndex = tempRef.targetIndex;
        return;
    }

    // Static fallback: look up the anchor name in the anchors table
    if (anchors.contains(dynRef.anchorName))
    {
        dynRef.targetIndex = anchors.at(dynRef.anchorName);
        return;
    }

    // URI with fragment (e.g., "extended#meta")
    const auto& ref{dynRef.originalRef};
    if (ref.contains(u'#'))
    {
        const auto hashPos{ref.lastIndexOf(u'#')};
        const auto fragment{ref.mid(hashPos + 1)};
        if (anchors.contains(fragment))
        {
            dynRef.targetIndex = anchors.at(fragment);
            return;
        }
        // Try scoped key
        const auto baseUri{ref.left(hashPos)};
        const auto lastSlash{baseUri.lastIndexOf(u'/')};
        const auto filename{lastSlash >= 0 ? baseUri.mid(lastSlash + 1) : baseUri};
        const auto scopedKey{filename + u"#"_qt_s + fragment};
        if (anchors.contains(scopedKey))
            dynRef.targetIndex = anchors.at(scopedKey);
    }
}

void resolveAllReferences(internal::CompiledSchema&                        compiled,
                          const std::unordered_map<QString, std::size_t>& anchors,
                          const QJsonValue&                               rootSchema,
                          CompileContext&                                  ctx)
{
    // Resolve in bounded passes: newly compiled schemas may introduce new refs
    static constexpr int kMaxPasses{8};
    std::size_t start{0};
    for (int pass{0}; pass < kMaxPasses; ++pass)
    {
        const auto end{compiled.nodes.size()};
        if (start >= end)
            break;
        for (std::size_t i{start}; i < end; ++i)
        {
            if (auto* refNode = std::get_if<RefSchema>(&compiled.nodes[i]))
                resolveReference(*refNode, compiled.rootIndex, anchors, rootSchema, ctx);
            else if (auto* dynRefNode = std::get_if<DynamicRefSchema>(&compiled.nodes[i]))
                resolveDynamicReference(*dynRefNode, compiled.rootIndex, anchors, rootSchema, ctx);
        }
        start = end;
        if (compiled.nodes.size() == end)
            break; // No new nodes added — done
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Three-Phase Compilation Pipeline
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief Phase 1: Build symbol table from definitions
 *
 * Compiles all $defs and definitions, registering them in the anchor table.
 * This creates a complete symbol table before any schema bodies reference them.
 *
 * @param compiled The compiled schema to populate with metadata
 * @param ctx Compilation context
 * @param schemaValue The root schema value
 * @return Success or error
 */
[[nodiscard]] std::expected<void, QueryError>
phase1_BuildSymbolTable(internal::CompiledSchema& compiled, CompileContext& ctx, const QJsonValue& schemaValue)
{
    using json_query::literals::operator""_qt_s;

    if (!schemaValue.isObject())
        return {};

    const auto rootObj{schemaValue.toObject()};

    // Extract root metadata ($id, $schema)
    extractRootMetadata(rootObj, compiled);

    // Compile all definitions and register them in the anchor table
    if (auto r{compileDefinitionsBlock(ctx, rootObj, u"$defs"_qt_s, u"#/$defs/"_qt_s)}; !r)
        return std::unexpected(r.error());

    if (auto r{compileDefinitionsBlock(ctx, rootObj, u"definitions"_qt_s, u"#/definitions/"_qt_s)}; !r)
        return std::unexpected(r.error());

    // Scan entire schema tree for $defs in inline schemas (if/then/else, properties, etc.)
    // This ensures $dynamicAnchor entries buried deep in the tree get registered
    scanDefsInSchemaTree(ctx, schemaValue);

    return {};
}

/**
 * @brief Phase 2: Generate code by compiling schema tree
 *
 * Recursively compiles the root schema and all nested schemas.
 * All definitions are already registered, so $ref can be resolved.
 *
 * @param compiled The compiled schema to populate with the root index
 * @param ctx Compilation context
 * @param schemaValue The root schema value
 * @return Success or error
 */
[[nodiscard]] std::expected<void, QueryError>
phase2_CompileSchemaTree(internal::CompiledSchema& compiled, CompileContext& ctx, const QJsonValue& schemaValue)
{
    auto rootResult{compileSchemaNode(ctx, schemaValue)};
    if (!rootResult)
        return std::unexpected(rootResult.error());

    compiled.rootIndex = *rootResult;
    return {};
}

/**
 * @brief Phase 3: Link references to their targets
 *
 * Resolves all $ref references to their target node indices using the anchor table.
 * JSON Pointer refs are resolved by walking the original schema document.
 */
void phase3_LinkReferences(internal::CompiledSchema&                        compiled,
                           const std::unordered_map<QString, std::size_t>& anchors,
                           const QJsonValue&                               rootSchema,
                           CompileContext&                                  ctx)
{
    resolveAllReferences(compiled, anchors, rootSchema, ctx);
}

} // anonymous namespace

// ────────────────────────────────────────────────────────────────────────────
// Public API: compileSchema
// ────────────────────────────────────────────────────────────────────────────

/**
 * @brief Compile a JSON Schema into an executable form
 *
 * This function implements a three-phase compilation pipeline based on classic
 * compiler theory for handling forward references and circular dependencies:
 *
 * **Phase 1: Symbol Table Construction**
 * - Extract root metadata ($id, $schema)
 * - Compile all $defs and register them in the anchor table
 * - This creates a complete symbol table before any schema bodies are compiled
 * - Prevents infinite recursion when definitions reference each other
 *
 * **Phase 2: Code Generation**
 * - Recursively compile the root schema and all nested schemas
 * - All $defs are already registered, so $ref can be resolved
 * - The keyword dispatcher processes validation keywords but skips $defs
 *   (since they were already compiled in Phase 1)
 *
 * **Phase 3: Reference Resolution (Linking)**
 * - Resolve all $ref references to their target node indices
 * - Handle various reference formats: JSON Pointer, anchors, URIs
 *
 * This two-pass approach is the standard solution in compiler design for
 * handling forward references. Attempting to compile $defs through the
 * recursive dispatcher would cause infinite loops when definitions are
 * mutually recursive.
 *
 * @param schemaValue The JSON Schema document (object or boolean)
 * @return Compiled schema or error
 */
std::expected<std::shared_ptr<internal::CompiledSchema>, QueryError> compileSchema(const QJsonValue& schemaValue)
{
    using json_query::literals::operator""_qt_s;

    // Validate input
    if (schemaValue.isNull() || schemaValue.isUndefined())
        return std::unexpected(QueryError(ParseError::EmptySchema));

    // Initialize compilation context
    auto           compiled{std::make_shared<internal::CompiledSchema>()};
    CompileContext ctx{compiled->nodes, compiled->anchors, compiled->dynamicAnchors, {}, {}};

    // Phase 1: Symbol Table Construction
    if (auto r{phase1_BuildSymbolTable(*compiled, ctx, schemaValue)}; !r)
        return std::unexpected(r.error());

    // Phase 2: Code Generation
    if (auto r{phase2_CompileSchemaTree(*compiled, ctx, schemaValue)}; !r)
        return std::unexpected(r.error());

    // Phase 3: Reference Resolution (Linking)
    phase3_LinkReferences(*compiled, ctx.anchors, schemaValue, ctx);

    // Phase 4: Build per-resource dynamic anchor maps and per-node anchor name map
    for (const auto& [resourceUri, anchorName, nodeIndex] : ctx.pendingDynamicAnchors)
    {
        // Find the resource root's node index
        std::size_t resourceNodeIndex{compiled->rootIndex}; // default: root resource
        if (!resourceUri.isEmpty())
        {
            if (const auto it{ctx.anchors.find(resourceUri)}; it != ctx.anchors.end())
                resourceNodeIndex = it->second;
        }
        compiled->resourceDynamicAnchors[resourceNodeIndex][anchorName] = nodeIndex;

        // Record per-node $dynamicAnchor name for bookending check
        compiled->nodeDynAnchorNames[nodeIndex] = anchorName;
    }

    return compiled;
}

} // namespace json_query::json_schema
