// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-schema/JSONSchemaCompile.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/json-schema/internal/CompileContext.hpp"
#include "json-query/json-schema/internal/CompileKeywords.hpp"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>

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
 * @brief Compile a single schema node (recursive)
 */
std::expected<std::size_t, QueryError> compileSchemaNode(CompileContext& ctx, const QJsonValue& schemaValue)
{
    // Boolean schema
    if (schemaValue.isBool())
        return ctx.addNode(BooleanSchema{schemaValue.toBool()});

    // Must be an object
    if (!schemaValue.isObject())
        return std::unexpected(QueryError(ParseError::InvalidSchemaStructure));

    const auto schemaObj{schemaValue.toObject()};

    // Save current base URI to restore after processing this subschema
    const auto savedBaseUri{ctx.baseUri};

    // Handle $id - changes the base URI for this subschema and descendants
    if (schemaObj.contains(u"$id"_qs) && schemaObj[u"$id"_qs].isString())
    {
        ctx.baseUri = schemaObj[u"$id"_qs].toString();
    }

    // Handle $anchor registration (scoped to current base URI)
    std::optional<QString> anchorName{};
    if (schemaObj.contains(u"$anchor"_qs) && schemaObj[u"$anchor"_qs].isString())
    {
        anchorName = schemaObj[u"$anchor"_qs].toString();
    }

    // In Draft 2020-12, $ref can be combined with other keywords
    // If $ref is present without other keywords, use RefSchema
    // If $ref is present with other keywords, we need to create an allOf with the ref and the additional constraints
    const auto hasRef{schemaObj.contains(u"$ref"_qs)};
    const auto refString{hasRef ? schemaObj[u"$ref"_qs].toString() : QString{}};
    
    // Check if there are other keywords besides $ref, $anchor, $id, $schema, $defs, title, description
    const auto hasOtherKeywords{[&]() {
        for (const auto& key : schemaObj.keys())
        {
            if (key != u"$ref"_qs && key != u"$anchor"_qs && key != u"$id"_qs && 
                key != u"$schema"_qs && key != u"$defs"_qs && key != u"definitions"_qs &&
                key != u"title"_qs && key != u"description"_qs && key != u"$comment"_qs)
            {
                return true;
            }
        }
        return false;
    }()};

    // If $ref is present without other keywords, create a simple RefSchema
    if (hasRef && !hasOtherKeywords)
    {
        ctx.baseUri = savedBaseUri;  // Restore before returning
        return ctx.addNode(RefSchema{0, refString});
    }

    ObjectSchema node{};
    
    // If $ref is present with other keywords, add the ref to allOf
    if (hasRef && hasOtherKeywords)
    {
        // Create a ref node for the reference
        const auto refNodeIndex{ctx.addNode(RefSchema{0, refString})};
        node.allOf.push_back(refNodeIndex);
    }

    // Type constraint
    auto typeResult{parseTypeKeyword(schemaObj[u"type"_qs])};
    if (!typeResult)
        return std::unexpected(typeResult.error());
    node.type = *typeResult;

    // Enum constraint
    auto enumResult{parseEnumKeyword(schemaObj[u"enum"_qs])};
    if (!enumResult)
        return std::unexpected(enumResult.error());
    node.enumValues = *enumResult;

    // Const constraint
    node.constValue = parseConstKeyword(schemaObj[u"const"_qs]);

    // String keywords
    auto patternResult{parsePatternKeyword(schemaObj[u"pattern"_qs])};
    if (!patternResult)
        return std::unexpected(patternResult.error());
    node.pattern = std::move(*patternResult);

    auto minLengthResult{parseIntegerKeyword(schemaObj[u"minLength"_qs])};
    if (!minLengthResult)
        return std::unexpected(minLengthResult.error());
    node.minLength = *minLengthResult;

    auto maxLengthResult{parseIntegerKeyword(schemaObj[u"maxLength"_qs])};
    if (!maxLengthResult)
        return std::unexpected(maxLengthResult.error());
    node.maxLength = *maxLengthResult;

    // Numeric keywords
    auto minimumResult{parseNumericKeyword(schemaObj[u"minimum"_qs])};
    if (!minimumResult)
        return std::unexpected(minimumResult.error());
    node.minimum = *minimumResult;

    auto maximumResult{parseNumericKeyword(schemaObj[u"maximum"_qs])};
    if (!maximumResult)
        return std::unexpected(maximumResult.error());
    node.maximum = *maximumResult;

    auto exclMinResult{parseNumericKeyword(schemaObj[u"exclusiveMinimum"_qs])};
    if (!exclMinResult)
        return std::unexpected(exclMinResult.error());
    node.exclusiveMinimum = *exclMinResult;

    auto exclMaxResult{parseNumericKeyword(schemaObj[u"exclusiveMaximum"_qs])};
    if (!exclMaxResult)
        return std::unexpected(exclMaxResult.error());
    node.exclusiveMaximum = *exclMaxResult;

    auto multipleOfResult{parseNumericKeyword(schemaObj[u"multipleOf"_qs])};
    if (!multipleOfResult)
        return std::unexpected(multipleOfResult.error());
    node.multipleOf = *multipleOfResult;

    // Array keywords
    auto minItemsResult{parseIntegerKeyword(schemaObj[u"minItems"_qs])};
    if (!minItemsResult)
        return std::unexpected(minItemsResult.error());
    node.minItems = *minItemsResult;

    auto maxItemsResult{parseIntegerKeyword(schemaObj[u"maxItems"_qs])};
    if (!maxItemsResult)
        return std::unexpected(maxItemsResult.error());
    node.maxItems = *maxItemsResult;

    if (schemaObj.contains(u"uniqueItems"_qs))
    {
        const auto uniqueVal{schemaObj[u"uniqueItems"_qs]};
        if (uniqueVal.isBool())
            node.uniqueItems = uniqueVal.toBool();
    }

    // Object keywords
    auto requiredResult{parseRequiredKeyword(schemaObj[u"required"_qs])};
    if (!requiredResult)
        return std::unexpected(requiredResult.error());
    node.required = std::move(*requiredResult);

    auto minPropsResult{parseIntegerKeyword(schemaObj[u"minProperties"_qs])};
    if (!minPropsResult)
        return std::unexpected(minPropsResult.error());
    node.minProperties = *minPropsResult;

    auto maxPropsResult{parseIntegerKeyword(schemaObj[u"maxProperties"_qs])};
    if (!maxPropsResult)
        return std::unexpected(maxPropsResult.error());
    node.maxProperties = *maxPropsResult;

    // Properties (requires recursive compilation)
    auto propertiesResult{compileProperties(ctx, schemaObj[u"properties"_qs])};
    if (!propertiesResult)
        return std::unexpected(propertiesResult.error());
    node.properties = std::move(*propertiesResult);

    // additionalProperties
    auto additionalPropsResult{compileOptionalSchema(ctx, schemaObj[u"additionalProperties"_qs])};
    if (!additionalPropsResult)
        return std::unexpected(additionalPropsResult.error());
    node.additionalProperties = *additionalPropsResult;

    // patternProperties
    if (schemaObj.contains(u"patternProperties"_qs))
    {
        const auto patternPropsValue{schemaObj[u"patternProperties"_qs]};
        if (patternPropsValue.isObject())
        {
            const auto patternPropsObj{patternPropsValue.toObject()};
            for (auto it = patternPropsObj.begin(); it != patternPropsObj.end(); ++it)
            {
                QRegularExpression regex{it.key()};
                if (!regex.isValid())
                    return std::unexpected(QueryError(ParseError::InvalidRegexPattern));
                regex.optimize();
                
                auto schemaResult{compileSchemaNode(ctx, it.value())};
                if (!schemaResult)
                    return std::unexpected(schemaResult.error());
                
                node.patternProperties.emplace_back(std::move(regex), *schemaResult);
            }
        }
    }

    // propertyNames
    auto propertyNamesResult{compileOptionalSchema(ctx, schemaObj[u"propertyNames"_qs])};
    if (!propertyNamesResult)
        return std::unexpected(propertyNamesResult.error());
    node.propertyNames = *propertyNamesResult;

    // items (2020-12 style - single schema)
    auto itemsResult{compileOptionalSchema(ctx, schemaObj[u"items"_qs])};
    if (!itemsResult)
        return std::unexpected(itemsResult.error());
    node.items = *itemsResult;

    // prefixItems
    auto prefixItemsResult{compileSchemaArray(ctx, schemaObj[u"prefixItems"_qs])};
    if (!prefixItemsResult)
        return std::unexpected(prefixItemsResult.error());
    node.prefixItems = std::move(*prefixItemsResult);

    // contains
    auto containsResult{compileOptionalSchema(ctx, schemaObj[u"contains"_qs])};
    if (!containsResult)
        return std::unexpected(containsResult.error());
    node.contains = *containsResult;

    // Combinators
    auto allOfResult{compileSchemaArray(ctx, schemaObj[u"allOf"_qs])};
    if (!allOfResult)
        return std::unexpected(allOfResult.error());
    node.allOf = std::move(*allOfResult);

    auto anyOfResult{compileSchemaArray(ctx, schemaObj[u"anyOf"_qs])};
    if (!anyOfResult)
        return std::unexpected(anyOfResult.error());
    node.anyOf = std::move(*anyOfResult);

    auto oneOfResult{compileSchemaArray(ctx, schemaObj[u"oneOf"_qs])};
    if (!oneOfResult)
        return std::unexpected(oneOfResult.error());
    node.oneOf = std::move(*oneOfResult);

    auto notResult{compileOptionalSchema(ctx, schemaObj[u"not"_qs])};
    if (!notResult)
        return std::unexpected(notResult.error());
    node.notSchema = *notResult;

    // Conditional
    auto ifResult{compileOptionalSchema(ctx, schemaObj[u"if"_qs])};
    if (!ifResult)
        return std::unexpected(ifResult.error());
    node.ifSchema = *ifResult;

    auto thenResult{compileOptionalSchema(ctx, schemaObj[u"then"_qs])};
    if (!thenResult)
        return std::unexpected(thenResult.error());
    node.thenSchema = *thenResult;

    auto elseResult{compileOptionalSchema(ctx, schemaObj[u"else"_qs])};
    if (!elseResult)
        return std::unexpected(elseResult.error());
    node.elseSchema = *elseResult;

    // Dependencies
    if (schemaObj.contains(u"dependentRequired"_qs))
    {
        const auto depReqValue{schemaObj[u"dependentRequired"_qs]};
        if (depReqValue.isObject())
        {
            const auto depReqObj{depReqValue.toObject()};
            for (auto it = depReqObj.begin(); it != depReqObj.end(); ++it)
            {
                if (it.value().isArray())
                {
                    const auto reqArray{it.value().toArray()};
                    std::vector<QString> requiredProps{};
                    for (const QJsonValue& req : reqArray)
                    {
                        if (req.isString())
                            requiredProps.push_back(req.toString());
                    }
                    node.dependentRequired[it.key()] = std::move(requiredProps);
                }
            }
        }
    }

    if (schemaObj.contains(u"dependentSchemas"_qs))
    {
        const auto depSchValue{schemaObj[u"dependentSchemas"_qs]};
        if (depSchValue.isObject())
        {
            const auto depSchObj{depSchValue.toObject()};
            for (auto it = depSchObj.begin(); it != depSchObj.end(); ++it)
            {
                auto schemaResult{compileSchemaNode(ctx, it.value())};
                if (!schemaResult)
                    return std::unexpected(schemaResult.error());
                node.dependentSchemas[it.key()] = *schemaResult;
            }
        }
    }

    // Process nested $defs (required for proper anchor resolution)
    if (schemaObj.contains(u"$defs"_qs))
    {
        const auto defsValue{schemaObj[u"$defs"_qs]};
        if (defsValue.isObject())
        {
            const auto defsObj{defsValue.toObject()};
            for (auto it = defsObj.begin(); it != defsObj.end(); ++it)
            {
                auto defResult{compileSchemaNode(ctx, it.value())};
                if (!defResult)
                    return std::unexpected(defResult.error());
            }
        }
    }

    // Metadata
    if (schemaObj.contains(u"title"_qs) && schemaObj[u"title"_qs].isString())
        node.title = schemaObj[u"title"_qs].toString();
    if (schemaObj.contains(u"description"_qs) && schemaObj[u"description"_qs].isString())
        node.description = schemaObj[u"description"_qs].toString();

    // Format (store as string, validation is optional per spec)
    if (schemaObj.contains(u"format"_qs) && schemaObj[u"format"_qs].isString())
        node.format = schemaObj[u"format"_qs].toString();

    // Add the node and register anchor if present
    const auto nodeIndex{ctx.addNode(std::move(node))};
    
    if (anchorName)
    {
        const auto scopedKey{ctx.scopedAnchorKey(*anchorName)};
        if (ctx.anchors.contains(scopedKey))
            return std::unexpected(QueryError(ParseError::DuplicateAnchor));
        ctx.anchors[scopedKey] = nodeIndex;
    }

    // Restore base URI before returning so sibling schemas get correct scope
    ctx.baseUri = savedBaseUri;
    
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
