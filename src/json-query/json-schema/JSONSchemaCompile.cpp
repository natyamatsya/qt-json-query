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
 * @brief Resolve a URI reference against a base URI (RFC 3986 §5.2)
 *
 * If ref is absolute (contains "://"), return as-is.
 * Otherwise resolve relative to base by replacing the last path segment.
 */
QString resolveUri(const QString& ref, const QString& base)
{
    using json_query::literals::operator""_qt_s;

    if (ref.contains(u"://"_qt_s) || base.isEmpty())
        return ref;

    // Absolute-path reference (starts with /)
    if (ref.startsWith(u'/'))
    {
        // Extract scheme + authority from base
        const auto schemeEnd{base.indexOf(u"://"_qt_s)};
        if (schemeEnd < 0)
            return ref;
        const auto authorityStart{schemeEnd + 3};
        const auto authorityEnd{base.indexOf(u'/', authorityStart)};
        if (authorityEnd < 0)
            return base + ref;
        return base.left(authorityEnd) + ref;
    }

    // Relative reference: replace last path segment in base
    const auto lastSlash{base.lastIndexOf(u'/')};
    if (lastSlash >= 0)
        return base.left(lastSlash + 1) + ref;

    return ref;
}

/**
 * @brief Extract $id and update base URI if present
 *
 * Resolves relative $id values against the current ctx.baseUri per RFC 3986.
 */
void processSchemaId(CompileContext& ctx, const QJsonObject& schemaObj, const QJsonValue& schemaValue)
{
    using json_query::literals::operator""_qt_s;

    if (schemaObj.contains(u"$id"_qt_s) && schemaObj[u"$id"_qt_s].isString())
    {
        const auto rawId{schemaObj[u"$id"_qt_s].toString()};
        ctx.baseUri = resolveUri(rawId, ctx.baseUri);
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

// Forward declaration: compileSchemaNode and compileDefinitionsBlock are mutually recursive
[[nodiscard]] std::expected<void, QueryError> compileDefinitionsBlock(CompileContext&    ctx,
                                                                      const QJsonObject& rootObj,
                                                                      const QString&     keyword,
                                                                      const QString&     pathPrefix);

/**
 * @brief Compile a single schema node (recursive worker function)
 *
 * Single-pass: processes $id → $defs → $anchor/$dynamicAnchor → $ref → keywords.
 * $defs are compiled inline after $id sets the base URI, ensuring correct
 * relative URI resolution at every nesting level.
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

    // Scope management
    BaseUriScope uriScope{ctx};
    processSchemaId(ctx, schemaObj, schemaValue);

    // Compile $defs inline (after $id sets ctx.baseUri, before $ref processing)
    // This ensures nested $defs see the correct resolved base URI at every level.
    if (auto r{compileDefinitionsBlock(ctx, schemaObj, u"$defs"_qt_s, u"#/$defs/"_qt_s)}; !r)
        return std::unexpected(r.error());
    if (auto r{compileDefinitionsBlock(ctx, schemaObj, u"definitions"_qt_s, u"#/definitions/"_qt_s)}; !r)
        return std::unexpected(r.error());

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

    // Helper: register $id (raw + resolved) and set selfIndex for fast-path nodes
    auto registerIdForFastPath = [&](std::size_t nodeIndex)
    {
        if (schemaObj.contains(u"$id"_qt_s) && schemaObj[u"$id"_qt_s].isString())
        {
            ctx.anchors[schemaObj[u"$id"_qt_s].toString()] = nodeIndex;
            if (!ctx.baseUri.isEmpty())
                ctx.anchors[ctx.baseUri] = nodeIndex;
        }
        // Set selfIndex for resource scope management
        if (auto* ref{std::get_if<RefSchema>(&ctx.nodes[nodeIndex])})
            ref->selfIndex = nodeIndex;
        else if (auto* dynRef{std::get_if<DynamicRefSchema>(&ctx.nodes[nodeIndex])})
            dynRef->selfIndex = nodeIndex;
        return nodeIndex;
    };

    if (hasRef && !hasDynRef && !hasValidationKeywords(schemaObj))
        return registerIdForFastPath(ctx.addNode(RefSchema{RefSchema::kUnresolved, qualifyRef(schemaObj[u"$ref"_qt_s].toString()), RefSchema::kNoIndex, ctx.baseUri}));

    if (hasDynRef && !hasRef && !hasValidationKeywords(schemaObj))
    {
        const auto dynRefStr{qualifyRef(schemaObj[u"$dynamicRef"_qt_s].toString())};
        auto fragment{dynRefStr};
        if (const auto hashPos{dynRefStr.indexOf(u'#')}; hashPos >= 0)
            fragment = dynRefStr.mid(hashPos + 1);
        return registerIdForFastPath(ctx.addNode(DynamicRefSchema{DynamicRefSchema::kUnresolved, fragment, DynamicRefSchema::kUnresolved, dynRefStr, ctx.baseUri}));
    }

    // Phase 3: Build ObjectSchema via dispatch
    ObjectSchema node{};

    if (hasRef)
        node.allOf.push_back(ctx.addNode(RefSchema{RefSchema::kUnresolved, qualifyRef(schemaObj[u"$ref"_qt_s].toString()), RefSchema::kNoIndex, ctx.baseUri}));

    if (hasDynRef)
    {
        const auto dynRefStr{qualifyRef(schemaObj[u"$dynamicRef"_qt_s].toString())};
        auto fragment{dynRefStr};
        if (const auto hashPos{dynRefStr.indexOf(u'#')}; hashPos >= 0)
            fragment = dynRefStr.mid(hashPos + 1);
        node.allOf.push_back(ctx.addNode(DynamicRefSchema{DynamicRefSchema::kUnresolved, fragment, DynamicRefSchema::kUnresolved, dynRefStr, ctx.baseUri}));
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

    // Register $id URI so $ref can resolve against it (both raw and resolved)
    if (schemaObj.contains(u"$id"_qt_s) && schemaObj[u"$id"_qt_s].isString())
    {
        ctx.anchors[schemaObj[u"$id"_qt_s].toString()] = nodeIndex;
        if (!ctx.baseUri.isEmpty())
            ctx.anchors[ctx.baseUri] = nodeIndex;
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
    }

    return {};
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
 * @brief Resolve a $ref string to a target node index
 *
 * Returns the resolved target index, or std::nullopt if unresolved.
 * IMPORTANT: This function may call compileSchemaNode which can reallocate
 * the nodes vector. Callers must NOT hold references into the vector across
 * this call — use the returned index to write back via the stable vector index.
 */
[[nodiscard]] std::optional<std::size_t>
resolveRef(const QString&                                   ref,
           const QString&                                   refBaseUri,
           std::size_t                                      rootIndex,
           const std::unordered_map<QString, std::size_t>&  anchors,
           const QJsonValue&                                rootSchema,
           CompileContext&                                   ctx)
{
    using json_query::literals::operator""_qt_s;

    // Root reference: "#" means the root of the current resource
    if (ref == u"#"_qt_s)
    {
        if (!refBaseUri.isEmpty() && anchors.contains(refBaseUri))
            return anchors.at(refBaseUri);
        return rootIndex;
    }

    // Direct anchor match
    if (anchors.contains(ref))
        return anchors.at(ref);

    // Local fragment reference (e.g., "#foo" or "#/properties/foo")
    if (ref.startsWith(u"#"_qt_s))
    {
        const auto fragment{ref.mid(1)};

        if (anchors.contains(fragment))
            return anchors.at(fragment);

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
                        return *compiled;
                }
            }
        }
        return std::nullopt;
    }

    // URI with fragment (e.g., "http://example.com/nested.json#foo" or "list#/$defs/items")
    if (ref.contains(u'#'))
    {
        const auto hashPos{ref.lastIndexOf(u'#')};
        auto baseUri{ref.left(hashPos)};
        const auto fragment{ref.mid(hashPos + 1)};

        // Try the full ref as a direct anchor match (e.g., scoped dynamic anchor key)
        if (anchors.contains(ref))
            return anchors.at(ref);

        // Resolve relative baseUri against refBaseUri for resource lookup
        // e.g., "bar" with refBaseUri "https://test.../main" → "https://test.../bar"
        if (!baseUri.contains(u"://"_qt_s) && refBaseUri.contains(u"://"_qt_s))
        {
            const auto resolved{resolveUri(baseUri, refBaseUri)};
            if (anchors.contains(resolved) || ctx.resourceDocuments.contains(resolved))
                baseUri = resolved;
        }

        // Try scoped anchor key (baseUri#fragment) — more specific than unscoped
        const auto scopedKey{baseUri + u"#"_qt_s + fragment};
        if (anchors.contains(scopedKey))
            return anchors.at(scopedKey);

        // Fallback: unscoped fragment lookup
        if (anchors.contains(fragment))
            return anchors.at(fragment);

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
                        if (resolved->isObject())
                        {
                            const auto resolvedObj{resolved->toObject()};
                            if (resolvedObj.contains(u"$id"_qt_s) && resolvedObj[u"$id"_qt_s].isString())
                            {
                                const auto resolvedId{resolvedObj[u"$id"_qt_s].toString()};
                                if (anchors.contains(resolvedId))
                                    return anchors.at(resolvedId);
                            }
                        }

                        // Set baseUri to containing resource for correct ref qualification
                        BaseUriScope resolutionScope{ctx};
                        ctx.baseUri = baseUri;

                        auto compiled{compileSchemaNode(ctx, *resolved)};
                        if (compiled)
                        {
                            // Set selfIndex to containing resource root so its scope is pushed
                            if (anchors.contains(baseUri))
                            {
                                const auto resourceRootIdx{anchors.at(baseUri)};
                                if (auto* newRef{std::get_if<RefSchema>(&ctx.nodes[*compiled])})
                                    newRef->selfIndex = resourceRootIdx;
                                else if (auto* newDynRef{std::get_if<DynamicRefSchema>(&ctx.nodes[*compiled])})
                                    newDynRef->selfIndex = resourceRootIdx;
                            }
                            return *compiled;
                        }
                    }
                }
            }
        }

        // Extract filename and build scoped key
        const auto lastSlash{baseUri.lastIndexOf(u'/')};
        const auto filename{lastSlash >= 0 ? baseUri.mid(lastSlash + 1) : baseUri};
        const auto filenameScopedKey{filename + u"#"_qt_s + fragment};

        if (anchors.contains(filenameScopedKey))
            return anchors.at(filenameScopedKey);
    }

    // Last resort: resolve relative URI against the compile-time base URI
    // e.g., ref "tree.json" with baseUri "http://host/dir/schema.json" → "http://host/dir/tree.json"
    if (!ref.startsWith(u'#') && !ref.contains(u"://"_qt_s) && refBaseUri.contains(u"://"_qt_s))
    {
        const auto resolved{resolveUri(ref, refBaseUri)};
        if (anchors.contains(resolved))
            return anchors.at(resolved);
    }

    return std::nullopt;
}

/**
 * @brief Fetch and compile a remote schema document via the SchemaFetcher callback
 *
 * Calls the user-provided fetcher to retrieve the schema at the given URI,
 * compiles it into the current context (registering anchors, $defs, etc.),
 * and registers the compiled root under the URI in the anchor table.
 *
 * @return true if the schema was fetched and compiled successfully
 */
bool fetchAndCompileRemoteSchema(CompileContext&                                  ctx,
                                 const QString&                                   uri,
                                 std::unordered_map<QString, std::size_t>&        anchors)
{
    using json_query::literals::operator""_qt_s;

    if (!ctx.fetcher)
        return false;

    // Avoid re-fetching URIs we already have
    if (ctx.resourceDocuments.contains(uri) || anchors.contains(uri))
        return false;

    const auto fetched{(*ctx.fetcher)(uri)};
    if (!fetched)
        return false;

    // Set base URI scope for the fetched document
    BaseUriScope uriScope{ctx};
    ctx.baseUri = uri;

    // Store the document for JSON Pointer resolution
    ctx.resourceDocuments[uri] = *fetched;

    // Compile the root of the fetched document (single-pass handles $defs inline)
    auto compiled{compileSchemaNode(ctx, *fetched)};
    if (!compiled)
        return false;

    // Register the compiled root under the URI
    anchors[uri] = *compiled;
    return true;
}

/**
 * @brief Resolve a DynamicRefSchema's static fallback target
 *
 * Returns the resolved target index, or std::nullopt if unresolved.
 * Takes originalRef and anchorName by value to avoid dangling references
 * (resolveRef may reallocate the nodes vector).
 */
[[nodiscard]] std::optional<std::size_t>
resolveDynRef(const QString&                                   originalRef,
              const QString&                                   anchorName,
              const QString&                                   refBaseUri,
              std::size_t                                      rootIndex,
              const std::unordered_map<QString, std::size_t>&  anchors,
              const QJsonValue&                                rootSchema,
              CompileContext&                                   ctx)
{
    using json_query::literals::operator""_qt_s;

    // Try resolving as a normal $ref first
    if (auto target{resolveRef(originalRef, refBaseUri, rootIndex, anchors, rootSchema, ctx)})
        return target;

    // Static fallback: look up the anchor name in the anchors table
    if (anchors.contains(anchorName))
        return anchors.at(anchorName);

    // URI with fragment (e.g., "extended#meta")
    if (originalRef.contains(u'#'))
    {
        const auto hashPos{originalRef.lastIndexOf(u'#')};
        const auto fragment{originalRef.mid(hashPos + 1)};
        if (anchors.contains(fragment))
            return anchors.at(fragment);

        const auto baseUri{originalRef.left(hashPos)};
        const auto lastSlash{baseUri.lastIndexOf(u'/')};
        const auto filename{lastSlash >= 0 ? baseUri.mid(lastSlash + 1) : baseUri};
        const auto scopedKey{filename + u"#"_qt_s + fragment};
        if (anchors.contains(scopedKey))
            return anchors.at(scopedKey);
    }

    return std::nullopt;
}

/**
 * @brief Extract the base URI from a $ref string (strip fragment)
 */
QString extractRefBaseUri(const QString& ref)
{
    if (ref.startsWith(u'#'))
        return {};
    const auto hashPos{ref.indexOf(u'#')};
    return hashPos >= 0 ? ref.left(hashPos) : ref;
}

void resolveAllReferences(internal::CompiledSchema&                        compiled,
                          std::unordered_map<QString, std::size_t>&        anchors,
                          const QJsonValue&                               rootSchema,
                          CompileContext&                                  ctx)
{
    // Resolve in bounded passes: newly compiled schemas may introduce new refs
    static constexpr int kMaxPasses{16};
    std::size_t start{0};
    for (int pass{0}; pass < kMaxPasses; ++pass)
    {
        const auto end{compiled.nodes.size()};
        if (start >= end)
            break;

        // Local resolution pass
        // NOTE: resolveRef/resolveDynRef may call compileSchemaNode which can
        // reallocate compiled.nodes. We copy ref strings BEFORE calling and
        // write back via the stable vector index, never through a pointer.
        for (std::size_t i{start}; i < end; ++i)
        {
            if (const auto* refNode{std::get_if<RefSchema>(&compiled.nodes[i])})
            {
                if (refNode->isResolved())
                    continue;
                const auto ref{refNode->originalRef}; // copy before potential realloc
                const auto base{refNode->baseUri};
                if (auto target{resolveRef(ref, base, compiled.rootIndex, anchors, rootSchema, ctx)})
                    std::get<RefSchema>(compiled.nodes[i]).targetIndex = *target;
            }
            else if (const auto* dynRefNode{std::get_if<DynamicRefSchema>(&compiled.nodes[i])})
            {
                if (dynRefNode->isResolved())
                    continue;
                const auto origRef{dynRefNode->originalRef};   // copy before potential realloc
                const auto anchor{dynRefNode->anchorName};
                const auto base{dynRefNode->baseUri};
                if (auto target{resolveDynRef(origRef, anchor, base, compiled.rootIndex, anchors, rootSchema, ctx)})
                    std::get<DynamicRefSchema>(compiled.nodes[i]).targetIndex = *target;
            }
        }

        // Remote fetch pass: try to fetch any still-unresolved refs
        if (ctx.fetcher)
        {
            bool fetched{false};
            for (std::size_t i{0}; i < compiled.nodes.size(); ++i)
            {
                QString uri{};
                QString base{};
                if (const auto* refNode{std::get_if<RefSchema>(&compiled.nodes[i])})
                {
                    if (!refNode->isResolved())
                    {
                        uri = extractRefBaseUri(refNode->originalRef);
                        base = refNode->baseUri;
                    }
                }
                else if (const auto* dynRefNode{std::get_if<DynamicRefSchema>(&compiled.nodes[i])})
                {
                    if (!dynRefNode->isResolved())
                    {
                        uri = extractRefBaseUri(dynRefNode->originalRef);
                        base = dynRefNode->baseUri;
                    }
                }
                if (!uri.isEmpty())
                {
                    using json_query::literals::operator""_qt_s;
                    // Resolve relative URI against compile-time base URI
                    const auto resolved{uri.contains(u"://"_qt_s) ? uri : resolveUri(uri, base)};
                    fetched |= fetchAndCompileRemoteSchema(ctx, resolved, anchors);
                }
            }
            if (fetched)
            {
                // Re-run local resolution from the beginning since new anchors are available
                start = 0;
                continue;
            }
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
 * @brief Phase 1: Extract root metadata ($id, $schema)
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

    return {};
}

/**
 * @brief Phase 2: Single-pass compilation of the schema tree
 *
 * Recursively compiles the root schema and all nested schemas.
 * Each compileSchemaNode call processes $id then $defs inline before $ref,
 * ensuring correct base URI propagation at every nesting level.
 * Forward references are resolved in Phase 3.
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
void phase3_LinkReferences(internal::CompiledSchema&                  compiled,
                           std::unordered_map<QString, std::size_t>& anchors,
                           const QJsonValue&                         rootSchema,
                           CompileContext&                            ctx)
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
 * Single-pass compilation pipeline inspired by nlohmann/json-schema-validator:
 *
 * **Phase 1: Root Metadata**
 * - Extract root $id and $schema dialect
 *
 * **Phase 2: Single-Pass Compilation**
 * - compileSchemaNode processes each schema: $id → $defs → $ref → keywords
 * - $defs are compiled inline after $id sets the base URI, ensuring correct
 *   relative URI resolution at every nesting level
 * - Forward references create unresolved RefSchema/DynamicRefSchema nodes
 *
 * **Phase 3: Reference Resolution (Linking)**
 * - Resolve all $ref/$dynamicRef to target node indices
 * - Handle JSON Pointer, anchor, URI, and remote references
 *
 * **Phase 4: Dynamic Anchor Registration**
 * - Build per-resource dynamic anchor maps for $dynamicRef resolution
 *
 * @param schemaValue The JSON Schema document (object or boolean)
 * @return Compiled schema or error
 */
std::expected<std::shared_ptr<internal::CompiledSchema>, QueryError>
compileSchema(const QJsonValue& schemaValue, SchemaFetcher fetcher)
{
    using json_query::literals::operator""_qt_s;

    // Validate input
    if (schemaValue.isNull() || schemaValue.isUndefined())
        return std::unexpected(QueryError(ParseError::EmptySchema));

    // Initialize compilation context
    auto           compiled{std::make_shared<internal::CompiledSchema>()};
    CompileContext ctx{compiled->nodes, compiled->anchors, compiled->dynamicAnchors, {}, {}, fetcher ? &fetcher : nullptr};

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
