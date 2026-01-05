// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-schema/JSONSchemaCompile.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/internal/CompileContext.hpp"
#include "json-query/json-schema/internal/CompileKeywords.hpp"
#include "json-query/json-schema/internal/CompileDispatch.hpp"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>

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

    const auto arr{arrayValue.toArray()};
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
    u"$ref", u"$anchor", u"$id", u"$schema", u"$defs", u"definitions", u"title", u"description", u"$comment"};

/**
 * @brief Check if schema contains validation keywords beyond metadata
 */
[[nodiscard]] bool hasValidationKeywords(const QJsonObject& schemaObj)
{
    return std::ranges::any_of(schemaObj.keys(), [](const QString& key)
                               { return std::ranges::none_of(kMetadataOnlyKeys, [&](const char16_t* mk)
                                                             { return key == mk; }); });
}

/**
 * @brief Extract $id and update base URI if present
 */
void processSchemaId(CompileContext& ctx, const QJsonObject& schemaObj)
{
    if (schemaObj.contains(u"$id"_qs) && schemaObj[u"$id"_qs].isString())
        ctx.baseUri = schemaObj[u"$id"_qs].toString();
}

/**
 * @brief Extract $anchor name if present
 */
[[nodiscard]] std::optional<QString> extractAnchorName(const QJsonObject& schemaObj)
{
    if (schemaObj.contains(u"$anchor"_qs) && schemaObj[u"$anchor"_qs].isString())
        return schemaObj[u"$anchor"_qs].toString();
    return std::nullopt;
}

/**
 * @brief Register anchor in context, checking for duplicates
 */
[[nodiscard]] std::expected<void, QueryError>
registerAnchor(CompileContext& ctx, const QString& anchorName, std::size_t nodeIndex)
{
    const auto scopedKey{ctx.scopedAnchorKey(anchorName)};
    if (ctx.anchors.contains(scopedKey))
        return std::unexpected(QueryError(ParseError::DuplicateAnchor));
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
 * @brief Compile a single schema node (recursive) - Clean pipeline architecture
 */
std::expected<std::size_t, QueryError> compileSchemaNode(CompileContext& ctx, const QJsonValue& schemaValue)
{
    // Fast path: Boolean schema
    if (schemaValue.isBool())
        return ctx.addNode(BooleanSchema{schemaValue.toBool()});

    if (!schemaValue.isObject())
        return std::unexpected(QueryError(ParseError::InvalidSchemaStructure));

    const auto schemaObj{schemaValue.toObject()};

    // Phase 1: Scope management
    BaseUriScope uriScope{ctx};
    processSchemaId(ctx, schemaObj);
    const auto anchorName{extractAnchorName(schemaObj)};

    // Phase 2: Fast path for $ref-only schemas
    const auto hasRef{schemaObj.contains(u"$ref"_qs)};
    if (hasRef && !hasValidationKeywords(schemaObj))
        return ctx.addNode(RefSchema{0, schemaObj[u"$ref"_qs].toString()});

    // Phase 3: Build ObjectSchema via dispatch
    ObjectSchema node{};

    if (hasRef)
        node.allOf.push_back(ctx.addNode(RefSchema{0, schemaObj[u"$ref"_qs].toString()}));

    if (auto r{FullKeywordDispatcher::dispatch(ctx, schemaObj, node, compileSchemaNode)}; !r)
        return std::unexpected(r.error());

    // Phase 4: Register node and anchor
    const auto nodeIndex{ctx.addNode(std::move(node))};

    if (anchorName)
    {
        if (auto r{registerAnchor(ctx, *anchorName, nodeIndex)}; !r)
            return std::unexpected(r.error());
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
    if (rootObj.contains(u"$id"_qs) && rootObj[u"$id"_qs].isString())
        compiled.schemaId = rootObj[u"$id"_qs].toString();

    if (rootObj.contains(u"$schema"_qs) && rootObj[u"$schema"_qs].isString())
        compiled.dialect = rootObj[u"$schema"_qs].toString();
}

/**
 * @brief Compile definitions block ($defs or definitions)
 */
[[nodiscard]] std::expected<void, QueryError>
compileDefinitionsBlock(CompileContext& ctx, const QJsonObject& rootObj, const QString& keyword, const QString& pathPrefix)
{
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
 * @brief Resolve a single $ref reference
 */
void resolveReference(RefSchema& refNode, std::size_t rootIndex, const std::unordered_map<QString, std::size_t>& anchors)
{
    const auto& ref{refNode.originalRef};

    // Root reference
    if (ref == u"#"_qs)
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

    // Anchor with # prefix (e.g., "#foo" → lookup "foo")
    if (ref.startsWith(u"#"_qs))
    {
        const auto anchorName{ref.mid(1)};
        if (anchors.contains(anchorName))
            refNode.targetIndex = anchors.at(anchorName);
        return;
    }

    // URI with fragment (e.g., "http://example.com/nested.json#foo")
    if (ref.contains(u'#'))
    {
        const auto hashPos{ref.lastIndexOf(u'#')};
        const auto baseUri{ref.left(hashPos)};
        const auto fragment{ref.mid(hashPos + 1)};

        // Extract filename and build scoped key
        const auto lastSlash{baseUri.lastIndexOf(u'/')};
        const auto filename{lastSlash >= 0 ? baseUri.mid(lastSlash + 1) : baseUri};
        const auto scopedKey{filename + u"#"_qs + fragment};

        if (anchors.contains(scopedKey))
            refNode.targetIndex = anchors.at(scopedKey);
    }
}

/**
 * @brief Second pass: resolve all $ref references in compiled nodes
 */
void resolveAllReferences(internal::CompiledSchema& compiled, const std::unordered_map<QString, std::size_t>& anchors)
{
    for (auto& node : compiled.nodes)
    {
        if (auto* refNode = std::get_if<RefSchema>(&node))
            resolveReference(*refNode, compiled.rootIndex, anchors);
    }
}

} // anonymous namespace

// ────────────────────────────────────────────────────────────────────────────
// Public API: compileSchema
// ────────────────────────────────────────────────────────────────────────────

std::expected<std::shared_ptr<internal::CompiledSchema>, QueryError> compileSchema(const QJsonValue& schemaValue)
{
    // Validate input
    if (schemaValue.isNull() || schemaValue.isUndefined())
        return std::unexpected(QueryError(ParseError::EmptySchema));

    // Initialize compilation context
    auto compiled{std::make_shared<internal::CompiledSchema>()};
    CompileContext ctx{compiled->nodes, compiled->anchors, compiled->dynamicAnchors, {}, {}};

    // Phase 1: Extract metadata and compile definitions
    if (schemaValue.isObject())
    {
        const auto rootObj{schemaValue.toObject()};

        extractRootMetadata(rootObj, *compiled);

        if (auto r{compileDefinitionsBlock(ctx, rootObj, u"$defs"_qs, u"#/$defs/"_qs)}; !r)
            return std::unexpected(r.error());

        if (auto r{compileDefinitionsBlock(ctx, rootObj, u"definitions"_qs, u"#/definitions/"_qs)}; !r)
            return std::unexpected(r.error());
    }

    // Phase 2: Compile root schema
    auto rootResult{compileSchemaNode(ctx, schemaValue)};
    if (!rootResult)
        return std::unexpected(rootResult.error());
    compiled->rootIndex = *rootResult;

    // Phase 3: Resolve $ref references
    resolveAllReferences(*compiled, ctx.anchors);

    return compiled;
}

} // namespace json_query::json_schema
