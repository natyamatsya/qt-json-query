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

/**
 * @brief Compile a single schema node (recursive) - TableGen-inspired dispatch architecture
 */
std::expected<std::size_t, QueryError> compileSchemaNode(CompileContext& ctx, const QJsonValue& schemaValue)
{
    // Fast path: Boolean schema
    if (schemaValue.isBool())
        return ctx.addNode(BooleanSchema{schemaValue.toBool()});

    // Must be an object
    if (!schemaValue.isObject())
        return std::unexpected(QueryError(ParseError::InvalidSchemaStructure));

    const auto schemaObj{schemaValue.toObject()};

    // RAII-style base URI management
    const auto savedBaseUri{ctx.baseUri};
    struct BaseUriRestorer
    {
        CompileContext& ctx;
        const QString&  saved;
        ~BaseUriRestorer() { ctx.baseUri = saved; }
    } restorer{ctx, savedBaseUri};

    // Handle $id - changes the base URI for this subschema and descendants
    if (schemaObj.contains(u"$id"_qs) && schemaObj[u"$id"_qs].isString())
        ctx.baseUri = schemaObj[u"$id"_qs].toString();

    // Handle $anchor registration (scoped to current base URI)
    std::optional<QString> anchorName{};
    if (schemaObj.contains(u"$anchor"_qs) && schemaObj[u"$anchor"_qs].isString())
        anchorName = schemaObj[u"$anchor"_qs].toString();

    // Fast path: $ref without other keywords → simple RefSchema
    const auto hasRef{schemaObj.contains(u"$ref"_qs)};
    if (hasRef)
    {
        const auto refString{schemaObj[u"$ref"_qs].toString()};

        // Check for other validation keywords
        static constexpr std::array metadataKeys{
            u"$ref", u"$anchor", u"$id", u"$schema", u"$defs", u"definitions", u"title", u"description", u"$comment"};

        const auto hasOtherKeywords{std::ranges::any_of(schemaObj.keys(), [](const QString& key)
                                                        { return std::ranges::none_of(metadataKeys, [&](const char16_t* mk)
                                                                                       { return key == mk; }); })};

        if (!hasOtherKeywords)
            return ctx.addNode(RefSchema{0, refString});
    }

    // ────────────────────────────────────────────────────────────────────────
    // TableGen-inspired dispatch: compile all keyword categories
    // ────────────────────────────────────────────────────────────────────────
    ObjectSchema node{};

    // If $ref with other keywords, add ref to allOf (Draft 2020-12 behavior)
    if (hasRef)
    {
        const auto refNodeIndex{ctx.addNode(RefSchema{0, schemaObj[u"$ref"_qs].toString()})};
        node.allOf.push_back(refNodeIndex);
    }

    // Dispatch to all keyword category handlers (pass compileSchemaNode as callback)
    if (auto r{FullKeywordDispatcher::dispatch(ctx, schemaObj, node, compileSchemaNode)}; !r)
        return std::unexpected(r.error());

    // Add the node and register anchor if present
    const auto nodeIndex{ctx.addNode(std::move(node))};

    if (anchorName)
    {
        const auto scopedKey{ctx.scopedAnchorKey(*anchorName)};
        if (ctx.anchors.contains(scopedKey))
            return std::unexpected(QueryError(ParseError::DuplicateAnchor));
        ctx.anchors[scopedKey] = nodeIndex;
    }

    return nodeIndex;
}

} // anonymous namespace

std::expected<std::shared_ptr<internal::CompiledSchema>, QueryError> compileSchema(const QJsonValue& schemaValue)
{
    if (schemaValue.isNull() || schemaValue.isUndefined())
        return std::unexpected(QueryError(ParseError::EmptySchema));

    auto compiled{std::make_shared<internal::CompiledSchema>()};

    CompileContext ctx{compiled->nodes, compiled->anchors, compiled->dynamicAnchors, {}, {}};

    // Extract metadata from root schema if it's an object
    if (schemaValue.isObject())
    {
        const auto rootObj{schemaValue.toObject()};
        if (rootObj.contains(u"$id"_qs) && rootObj[u"$id"_qs].isString())
            compiled->schemaId = rootObj[u"$id"_qs].toString();
        if (rootObj.contains(u"$schema"_qs) && rootObj[u"$schema"_qs].isString())
            compiled->dialect = rootObj[u"$schema"_qs].toString();
        
        // Compile $defs first so they're available for $ref resolution
        if (rootObj.contains(u"$defs"_qs))
        {
            const auto defsValue{rootObj[u"$defs"_qs]};
            if (defsValue.isObject())
            {
                const auto defsObj{defsValue.toObject()};
                for (auto it = defsObj.begin(); it != defsObj.end(); ++it)
                {
                    auto defResult{compileSchemaNode(ctx, it.value())};
                    if (!defResult)
                        return std::unexpected(defResult.error());
                    
                    // Register the definition with its JSON Pointer path
                    const auto defPath{u"#/$defs/"_qs + it.key()};
                    ctx.anchors[defPath] = *defResult;
                }
            }
        }
        
        // Also handle legacy "definitions" keyword
        if (rootObj.contains(u"definitions"_qs))
        {
            const auto defsValue{rootObj[u"definitions"_qs]};
            if (defsValue.isObject())
            {
                const auto defsObj{defsValue.toObject()};
                for (auto it = defsObj.begin(); it != defsObj.end(); ++it)
                {
                    auto defResult{compileSchemaNode(ctx, it.value())};
                    if (!defResult)
                        return std::unexpected(defResult.error());
                    
                    // Register with legacy path
                    const auto defPath{u"#/definitions/"_qs + it.key()};
                    ctx.anchors[defPath] = *defResult;
                }
            }
        }
    }

    // Compile the root schema
    auto rootResult{compileSchemaNode(ctx, schemaValue)};
    if (!rootResult)
        return std::unexpected(rootResult.error());

    compiled->rootIndex = *rootResult;
    
    // Second pass: resolve $ref references
    for (std::size_t i = 0; i < compiled->nodes.size(); ++i)
    {
        if (std::holds_alternative<RefSchema>(compiled->nodes[i]))
        {
            auto& refNode{std::get<RefSchema>(compiled->nodes[i])};
            const auto& refString{refNode.originalRef};
            
            // Try to resolve the reference
            if (refString == u"#"_qs)
            {
                refNode.targetIndex = compiled->rootIndex;
            }
            else if (ctx.anchors.contains(refString))
            {
                refNode.targetIndex = ctx.anchors[refString];
            }
            else if (refString.startsWith(u"#"_qs))
            {
                // Try without the # prefix for anchor lookup
                const auto anchorName{refString.mid(1)};
                if (ctx.anchors.contains(anchorName))
                {
                    refNode.targetIndex = ctx.anchors[anchorName];
                }
            }
            else if (refString.contains(u'#'))
            {
                // URI with fragment - try to resolve using the base URI and anchor
                // e.g., "http://example.com/nested.json#foo" -> look for "nested.json#foo"
                const auto hashPos{refString.lastIndexOf(u'#')};
                const auto baseUri{refString.left(hashPos)};
                const auto fragment{refString.mid(hashPos + 1)};
                
                // Try scoped lookup: extract just the filename from the URI and combine with fragment
                const auto lastSlash{baseUri.lastIndexOf(u'/')};
                const auto filename{lastSlash >= 0 ? baseUri.mid(lastSlash + 1) : baseUri};
                const auto scopedKey{filename + u"#"_qs + fragment};
                
                if (ctx.anchors.contains(scopedKey))
                {
                    refNode.targetIndex = ctx.anchors[scopedKey];
                }
            }
        }
    }

    return compiled;
}

} // namespace json_query::json_schema
