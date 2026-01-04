// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-schema/JSONSchemaCompile.hpp"
#include "json-query/json-schema/JSONSchemaError.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"

#include <QtCore/QJsonArray>
#include <QtCore/QJsonObject>
#include <QtCore/QRegularExpression>

#include <unordered_map>

namespace json_query::json_schema
{

namespace
{

using namespace internal;

/**
 * @brief Context for schema compilation
 *
 * Maintains state during recursive schema compilation.
 */
struct CompileContext
{
    std::vector<SchemaNode>&                  nodes;
    std::unordered_map<QString, std::size_t>& anchors;
    std::unordered_map<QString, std::size_t>& dynamicAnchors;
    QString                                   basePath{};

    // Add a node and return its index
    std::size_t addNode(SchemaNode node)
    {
        const auto index{nodes.size()};
        nodes.push_back(std::move(node));
        return index;
    }
};

/**
 * @brief Parse the 'type' keyword
 */
std::expected<std::optional<TypeConstraint>, QueryError> parseTypeKeyword(const QJsonValue& typeValue)
{
    if (typeValue.isUndefined())
        return std::nullopt;

    TypeConstraint constraint{};

    if (typeValue.isString())
    {
        auto schemaType{stringToSchemaType(typeValue.toString())};
        if (!schemaType)
            return std::unexpected(QueryError(ParseError::InvalidTypeValue));
        constraint.allowedTypes.push_back(*schemaType);
    }
    else if (typeValue.isArray())
    {
        const auto typeArray{typeValue.toArray()};
        if (typeArray.isEmpty())
            return std::unexpected(QueryError(ParseError::InvalidTypeValue));

        for (const QJsonValue& typeItem : typeArray)
        {
            if (!typeItem.isString())
                return std::unexpected(QueryError(ParseError::InvalidTypeValue));
            auto schemaType{stringToSchemaType(typeItem.toString())};
            if (!schemaType)
                return std::unexpected(QueryError(ParseError::InvalidTypeValue));
            constraint.allowedTypes.push_back(*schemaType);
        }
    }
    else
    {
        return std::unexpected(QueryError(ParseError::InvalidTypeValue));
    }

    return constraint;
}

/**
 * @brief Parse the 'enum' keyword
 */
std::expected<std::optional<QJsonArray>, QueryError> parseEnumKeyword(const QJsonValue& enumValue)
{
    if (enumValue.isUndefined())
        return std::nullopt;

    if (!enumValue.isArray())
        return std::unexpected(QueryError(ParseError::InvalidEnumValue));

    const auto enumArray{enumValue.toArray()};
    if (enumArray.isEmpty())
        return std::unexpected(QueryError(ParseError::InvalidEnumValue));

    return enumArray;
}

/**
 * @brief Parse the 'const' keyword
 */
std::optional<QJsonValue> parseConstKeyword(const QJsonValue& constValue)
{
    if (constValue.isUndefined())
        return std::nullopt;
    return constValue;
}

/**
 * @brief Parse numeric keywords (minimum, maximum, etc.)
 */
std::expected<std::optional<double>, QueryError> parseNumericKeyword(const QJsonValue&            value,
                                                                     [[maybe_unused]] const char* keywordName)
{
    if (value.isUndefined())
        return std::nullopt;

    if (!value.isDouble())
        return std::unexpected(QueryError(ParseError::InvalidKeywordValue));

    return value.toDouble();
}

/**
 * @brief Parse integer keywords (minLength, maxLength, minItems, etc.)
 */
std::expected<std::optional<std::size_t>, QueryError> parseIntegerKeyword(const QJsonValue&            value,
                                                                          [[maybe_unused]] const char* keywordName)
{
    if (value.isUndefined())
        return std::nullopt;

    if (!value.isDouble())
        return std::unexpected(QueryError(ParseError::InvalidKeywordValue));

    const auto d{value.toDouble()};
    if (d < 0 || d != static_cast<double>(static_cast<std::size_t>(d)))
        return std::unexpected(QueryError(ParseError::InvalidKeywordValue));

    return static_cast<std::size_t>(d);
}

/**
 * @brief Parse the 'pattern' keyword
 */
std::expected<std::optional<QRegularExpression>, QueryError> parsePatternKeyword(const QJsonValue& patternValue)
{
    if (patternValue.isUndefined())
        return std::nullopt;

    if (!patternValue.isString())
        return std::unexpected(QueryError(ParseError::InvalidKeywordValue));

    QRegularExpression regex(patternValue.toString());
    if (!regex.isValid())
        return std::unexpected(QueryError(ParseError::InvalidRegexPattern));

    regex.optimize(); // Enable JIT for better performance
    return regex;
}

/**
 * @brief Parse the 'required' keyword
 */
std::expected<std::vector<QString>, QueryError> parseRequiredKeyword(const QJsonValue& requiredValue)
{
    std::vector<QString> result{};

    if (requiredValue.isUndefined())
        return result;

    if (!requiredValue.isArray())
        return std::unexpected(QueryError(ParseError::InvalidKeywordValue));

    const auto requiredArray{requiredValue.toArray()};
    result.reserve(static_cast<std::size_t>(requiredArray.size()));

    for (const QJsonValue& item : requiredArray)
    {
        if (!item.isString())
            return std::unexpected(QueryError(ParseError::InvalidKeywordValue));
        result.push_back(item.toString());
    }

    return result;
}

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

    // Handle $ref - if present, it should be the only keyword that matters (per spec)
    if (schemaObj.contains(u"$ref"_qs))
    {
        const auto refString{schemaObj[u"$ref"_qs].toString()};
        
        // For now, create a placeholder RefSchema that will be resolved later
        // In a two-pass system, we'd resolve this after all schemas are compiled
        // For simplicity, we'll try to resolve it immediately if it's a simple reference
        
        // Check if it's a reference to root
        if (refString == u"#"_qs)
        {
            // Reference to root - we'll use index 0 (will be set as rootIndex)
            return ctx.addNode(RefSchema{0, refString});
        }
        
        // Check if it's a reference to an anchor
        if (refString.startsWith(u"#"_qs) && !refString.contains(u"/"_qs))
        {
            const auto anchorName{refString.mid(1)};
            if (ctx.anchors.contains(anchorName))
            {
                return ctx.addNode(RefSchema{ctx.anchors[anchorName], refString});
            }
            // Anchor not found yet - it might be defined later
            // For now, create a placeholder
            return ctx.addNode(RefSchema{0, refString});
        }
        
        // For JSON Pointer style references (e.g., #/$defs/foo), we need to defer resolution
        // Create a RefSchema node with a placeholder index
        return ctx.addNode(RefSchema{0, refString});
    }

    // Handle $anchor registration
    std::optional<QString> anchorName{};
    if (schemaObj.contains(u"$anchor"_qs) && schemaObj[u"$anchor"_qs].isString())
    {
        anchorName = schemaObj[u"$anchor"_qs].toString();
    }

    ObjectSchema node{};

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

    auto minLengthResult{parseIntegerKeyword(schemaObj[u"minLength"_qs], "minLength")};
    if (!minLengthResult)
        return std::unexpected(minLengthResult.error());
    node.minLength = *minLengthResult;

    auto maxLengthResult{parseIntegerKeyword(schemaObj[u"maxLength"_qs], "maxLength")};
    if (!maxLengthResult)
        return std::unexpected(maxLengthResult.error());
    node.maxLength = *maxLengthResult;

    // Numeric keywords
    auto minimumResult{parseNumericKeyword(schemaObj[u"minimum"_qs], "minimum")};
    if (!minimumResult)
        return std::unexpected(minimumResult.error());
    node.minimum = *minimumResult;

    auto maximumResult{parseNumericKeyword(schemaObj[u"maximum"_qs], "maximum")};
    if (!maximumResult)
        return std::unexpected(maximumResult.error());
    node.maximum = *maximumResult;

    auto exclMinResult{parseNumericKeyword(schemaObj[u"exclusiveMinimum"_qs], "exclusiveMinimum")};
    if (!exclMinResult)
        return std::unexpected(exclMinResult.error());
    node.exclusiveMinimum = *exclMinResult;

    auto exclMaxResult{parseNumericKeyword(schemaObj[u"exclusiveMaximum"_qs], "exclusiveMaximum")};
    if (!exclMaxResult)
        return std::unexpected(exclMaxResult.error());
    node.exclusiveMaximum = *exclMaxResult;

    auto multipleOfResult{parseNumericKeyword(schemaObj[u"multipleOf"_qs], "multipleOf")};
    if (!multipleOfResult)
        return std::unexpected(multipleOfResult.error());
    node.multipleOf = *multipleOfResult;

    // Array keywords
    auto minItemsResult{parseIntegerKeyword(schemaObj[u"minItems"_qs], "minItems")};
    if (!minItemsResult)
        return std::unexpected(minItemsResult.error());
    node.minItems = *minItemsResult;

    auto maxItemsResult{parseIntegerKeyword(schemaObj[u"maxItems"_qs], "maxItems")};
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

    auto minPropsResult{parseIntegerKeyword(schemaObj[u"minProperties"_qs], "minProperties")};
    if (!minPropsResult)
        return std::unexpected(minPropsResult.error());
    node.minProperties = *minPropsResult;

    auto maxPropsResult{parseIntegerKeyword(schemaObj[u"maxProperties"_qs], "maxProperties")};
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
        if (ctx.anchors.contains(*anchorName))
            return std::unexpected(QueryError(ParseError::DuplicateAnchor));
        ctx.anchors[*anchorName] = nodeIndex;
    }
    
    return nodeIndex;
}

} // anonymous namespace

std::expected<std::shared_ptr<internal::CompiledSchema>, QueryError> compileSchema(const QJsonValue& schemaValue)
{
    if (schemaValue.isNull() || schemaValue.isUndefined())
        return std::unexpected(QueryError(ParseError::EmptySchema));

    auto compiled{std::make_shared<internal::CompiledSchema>()};

    CompileContext ctx{compiled->nodes, compiled->anchors, compiled->dynamicAnchors, {}};

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
                    const auto defName{it.key()};
                    auto defResult{compileSchemaNode(ctx, it.value())};
                    if (!defResult)
                        return std::unexpected(defResult.error());
                    
                    // Register the definition with its JSON Pointer path
                    const auto defPath{u"#/$defs/"_qs + defName};
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
                    const auto defName{it.key()};
                    auto defResult{compileSchemaNode(ctx, it.value())};
                    if (!defResult)
                        return std::unexpected(defResult.error());
                    
                    // Register with legacy path
                    const auto defPath{u"#/definitions/"_qs + defName};
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
        }
    }

    return compiled;
}

} // namespace json_query::json_schema
