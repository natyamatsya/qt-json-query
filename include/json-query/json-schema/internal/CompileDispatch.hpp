// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/CompileContext.hpp"
#include "json-query/json-schema/internal/CompileKeywords.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"

#include <QJsonObject>
#include <expected>

namespace json_query::json_schema::internal
{

/**
 * @brief Callback type for recursive schema compilation
 *
 * Similar to ValidateNodeFn, this breaks the circular dependency
 * by allowing the dispatch handlers to call back into the main compiler.
 */
using CompileSchemaFn = std::expected<std::size_t, QueryError>(CompileContext&, const QJsonValue&);

/**
 * @brief Helper macro replacement: try parsing a keyword and assign result
 *
 * Uses C++23 monadic operations for clean error propagation.
 */
template <typename T, typename Parser>
[[nodiscard]] inline std::expected<void, QueryError>
tryParseKeyword(const QJsonObject& obj, const QString& key, std::optional<T>& target, Parser parser)
{
    auto result{parser(obj[key])};
    if (!result)
        return std::unexpected(result.error());
    target = std::move(*result);
    return {};
}

/**
 * @brief Compile string-related keywords (pattern, minLength, maxLength, format)
 */
[[nodiscard]] inline std::expected<void, QueryError>
compileStringKeywords(const QJsonObject& schemaObj, ObjectSchema& node)
{
    if (auto r{parsePatternKeyword(schemaObj[u"pattern"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.pattern = std::move(*r);

    if (auto r{parseIntegerKeyword(schemaObj[u"minLength"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.minLength = *r;

    if (auto r{parseIntegerKeyword(schemaObj[u"maxLength"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.maxLength = *r;

    // Format is just a string, no parsing needed
    if (schemaObj.contains(u"format"_qs) && schemaObj[u"format"_qs].isString())
        node.format = schemaObj[u"format"_qs].toString();

    return {};
}

/**
 * @brief Compile numeric-related keywords (minimum, maximum, exclusiveMinimum, exclusiveMaximum, multipleOf)
 */
[[nodiscard]] inline std::expected<void, QueryError>
compileNumericKeywords(const QJsonObject& schemaObj, ObjectSchema& node)
{
    if (auto r{parseNumericKeyword(schemaObj[u"minimum"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.minimum = *r;

    if (auto r{parseNumericKeyword(schemaObj[u"maximum"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.maximum = *r;

    if (auto r{parseNumericKeyword(schemaObj[u"exclusiveMinimum"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.exclusiveMinimum = *r;

    if (auto r{parseNumericKeyword(schemaObj[u"exclusiveMaximum"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.exclusiveMaximum = *r;

    if (auto r{parseNumericKeyword(schemaObj[u"multipleOf"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.multipleOf = *r;

    return {};
}

/**
 * @brief Compile array-related keywords (minItems, maxItems, uniqueItems, prefixItems, items, contains)
 */
[[nodiscard]] inline std::expected<void, QueryError>
compileArrayKeywords(CompileContext& ctx, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn& compile)
{
    if (auto r{parseIntegerKeyword(schemaObj[u"minItems"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.minItems = *r;

    if (auto r{parseIntegerKeyword(schemaObj[u"maxItems"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.maxItems = *r;

    if (schemaObj.contains(u"uniqueItems"_qs))
    {
        const auto uniqueVal{schemaObj[u"uniqueItems"_qs]};
        if (uniqueVal.isBool())
            node.uniqueItems = uniqueVal.toBool();
    }

    // prefixItems - array of schemas
    if (schemaObj.contains(u"prefixItems"_qs) && schemaObj[u"prefixItems"_qs].isArray())
    {
        for (const auto& item : schemaObj[u"prefixItems"_qs].toArray())
        {
            auto r{compile(ctx, item)};
            if (!r)
                return std::unexpected(r.error());
            node.prefixItems.push_back(*r);
        }
    }

    // items - single schema for additional items
    if (schemaObj.contains(u"items"_qs))
    {
        auto r{compile(ctx, schemaObj[u"items"_qs])};
        if (!r)
            return std::unexpected(r.error());
        node.items = *r;
    }

    // contains
    if (schemaObj.contains(u"contains"_qs))
    {
        auto r{compile(ctx, schemaObj[u"contains"_qs])};
        if (!r)
            return std::unexpected(r.error());
        node.contains = *r;
    }

    return {};
}

/**
 * @brief Compile object-related keywords (properties, required, additionalProperties, etc.)
 */
[[nodiscard]] inline std::expected<void, QueryError>
compileObjectKeywords(CompileContext& ctx, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn& compile)
{
    // required
    if (auto r{parseRequiredKeyword(schemaObj[u"required"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.required = std::move(*r);

    // minProperties, maxProperties
    if (auto r{parseIntegerKeyword(schemaObj[u"minProperties"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.minProperties = *r;

    if (auto r{parseIntegerKeyword(schemaObj[u"maxProperties"_qs])}; !r)
        return std::unexpected(r.error());
    else
        node.maxProperties = *r;

    // properties - map of property schemas
    if (schemaObj.contains(u"properties"_qs) && schemaObj[u"properties"_qs].isObject())
    {
        const auto propsObj{schemaObj[u"properties"_qs].toObject()};
        for (auto it = propsObj.begin(); it != propsObj.end(); ++it)
        {
            auto r{compile(ctx, it.value())};
            if (!r)
                return std::unexpected(r.error());
            node.properties[it.key()] = *r;
        }
    }

    // additionalProperties
    if (schemaObj.contains(u"additionalProperties"_qs))
    {
        auto r{compile(ctx, schemaObj[u"additionalProperties"_qs])};
        if (!r)
            return std::unexpected(r.error());
        node.additionalProperties = *r;
    }

    // patternProperties
    if (schemaObj.contains(u"patternProperties"_qs) && schemaObj[u"patternProperties"_qs].isObject())
    {
        const auto patternPropsObj{schemaObj[u"patternProperties"_qs].toObject()};
        for (auto it = patternPropsObj.begin(); it != patternPropsObj.end(); ++it)
        {
            QRegularExpression regex{it.key()};
            if (!regex.isValid())
                return std::unexpected(QueryError(ParseError::InvalidRegexPattern));
            regex.optimize();

            auto r{compile(ctx, it.value())};
            if (!r)
                return std::unexpected(r.error());

            node.patternProperties.emplace_back(std::move(regex), *r);
        }
    }

    // propertyNames
    if (schemaObj.contains(u"propertyNames"_qs))
    {
        auto r{compile(ctx, schemaObj[u"propertyNames"_qs])};
        if (!r)
            return std::unexpected(r.error());
        node.propertyNames = *r;
    }

    // dependentRequired
    if (schemaObj.contains(u"dependentRequired"_qs) && schemaObj[u"dependentRequired"_qs].isObject())
    {
        const auto depReqObj{schemaObj[u"dependentRequired"_qs].toObject()};
        for (auto it = depReqObj.begin(); it != depReqObj.end(); ++it)
        {
            if (it.value().isArray())
            {
                std::vector<QString> requiredProps{};
                for (const QJsonValue& req : it.value().toArray())
                {
                    if (req.isString())
                        requiredProps.push_back(req.toString());
                }
                node.dependentRequired[it.key()] = std::move(requiredProps);
            }
        }
    }

    // dependentSchemas
    if (schemaObj.contains(u"dependentSchemas"_qs) && schemaObj[u"dependentSchemas"_qs].isObject())
    {
        const auto depSchObj{schemaObj[u"dependentSchemas"_qs].toObject()};
        for (auto it = depSchObj.begin(); it != depSchObj.end(); ++it)
        {
            auto r{compile(ctx, it.value())};
            if (!r)
                return std::unexpected(r.error());
            node.dependentSchemas[it.key()] = *r;
        }
    }

    return {};
}

/**
 * @brief Compile combinator keywords (allOf, anyOf, oneOf, not, if/then/else)
 */
[[nodiscard]] inline std::expected<void, QueryError>
compileCombinatorKeywords(CompileContext& ctx, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn& compile)
{
    // allOf
    if (schemaObj.contains(u"allOf"_qs) && schemaObj[u"allOf"_qs].isArray())
    {
        for (const auto& item : schemaObj[u"allOf"_qs].toArray())
        {
            auto r{compile(ctx, item)};
            if (!r)
                return std::unexpected(r.error());
            node.allOf.push_back(*r);
        }
    }

    // anyOf
    if (schemaObj.contains(u"anyOf"_qs) && schemaObj[u"anyOf"_qs].isArray())
    {
        for (const auto& item : schemaObj[u"anyOf"_qs].toArray())
        {
            auto r{compile(ctx, item)};
            if (!r)
                return std::unexpected(r.error());
            node.anyOf.push_back(*r);
        }
    }

    // oneOf
    if (schemaObj.contains(u"oneOf"_qs) && schemaObj[u"oneOf"_qs].isArray())
    {
        for (const auto& item : schemaObj[u"oneOf"_qs].toArray())
        {
            auto r{compile(ctx, item)};
            if (!r)
                return std::unexpected(r.error());
            node.oneOf.push_back(*r);
        }
    }

    // not
    if (schemaObj.contains(u"not"_qs))
    {
        auto r{compile(ctx, schemaObj[u"not"_qs])};
        if (!r)
            return std::unexpected(r.error());
        node.notSchema = *r;
    }

    // if/then/else
    if (schemaObj.contains(u"if"_qs))
    {
        auto r{compile(ctx, schemaObj[u"if"_qs])};
        if (!r)
            return std::unexpected(r.error());
        node.ifSchema = *r;
    }

    if (schemaObj.contains(u"then"_qs))
    {
        auto r{compile(ctx, schemaObj[u"then"_qs])};
        if (!r)
            return std::unexpected(r.error());
        node.thenSchema = *r;
    }

    if (schemaObj.contains(u"else"_qs))
    {
        auto r{compile(ctx, schemaObj[u"else"_qs])};
        if (!r)
            return std::unexpected(r.error());
        node.elseSchema = *r;
    }

    return {};
}

/**
 * @brief Compile metadata keywords (title, description)
 */
inline void compileMetadataKeywords(const QJsonObject& schemaObj, ObjectSchema& node)
{
    if (schemaObj.contains(u"title"_qs) && schemaObj[u"title"_qs].isString())
        node.title = schemaObj[u"title"_qs].toString();

    if (schemaObj.contains(u"description"_qs) && schemaObj[u"description"_qs].isString())
        node.description = schemaObj[u"description"_qs].toString();
}

/**
 * @brief Process nested $defs for anchor resolution
 */
[[nodiscard]] inline std::expected<void, QueryError>
compileNestedDefs(CompileContext& ctx, const QJsonObject& schemaObj, CompileSchemaFn& compile)
{
    if (schemaObj.contains(u"$defs"_qs) && schemaObj[u"$defs"_qs].isObject())
    {
        const auto defsObj{schemaObj[u"$defs"_qs].toObject()};
        for (auto it = defsObj.begin(); it != defsObj.end(); ++it)
        {
            auto r{compile(ctx, it.value())};
            if (!r)
                return std::unexpected(r.error());
        }
    }
    return {};
}

// ────────────────────────────────────────────────────────────────────────────
// TableGen-Inspired Keyword Category Dispatch
// ────────────────────────────────────────────────────────────────────────────

enum class KeywordCategory
{
    TypeConstraints,  // type, enum, const
    StringKeywords,   // pattern, minLength, maxLength, format
    NumericKeywords,  // minimum, maximum, exclusiveMinimum, exclusiveMaximum, multipleOf
    ArrayKeywords,    // minItems, maxItems, uniqueItems, prefixItems, items, contains
    ObjectKeywords,   // properties, required, additionalProperties, patternProperties, etc.
    Combinators,      // allOf, anyOf, oneOf, not, if/then/else
    Metadata,         // title, description
    NestedDefs        // $defs processing
};

template <KeywordCategory Category>
struct KeywordCategoryHandler;

template <>
struct KeywordCategoryHandler<KeywordCategory::TypeConstraints>
{
    [[nodiscard]] static std::expected<void, QueryError>
    compile(CompileContext&, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn&)
    {
        if (auto r{parseTypeKeyword(schemaObj[u"type"_qs])}; !r)
            return std::unexpected(r.error());
        else
            node.type = *r;

        if (auto r{parseEnumKeyword(schemaObj[u"enum"_qs])}; !r)
            return std::unexpected(r.error());
        else
            node.enumValues = *r;

        node.constValue = parseConstKeyword(schemaObj[u"const"_qs]);
        return {};
    }
};

template <>
struct KeywordCategoryHandler<KeywordCategory::StringKeywords>
{
    [[nodiscard]] static std::expected<void, QueryError>
    compile(CompileContext&, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn&)
    {
        return compileStringKeywords(schemaObj, node);
    }
};

template <>
struct KeywordCategoryHandler<KeywordCategory::NumericKeywords>
{
    [[nodiscard]] static std::expected<void, QueryError>
    compile(CompileContext&, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn&)
    {
        return compileNumericKeywords(schemaObj, node);
    }
};

template <>
struct KeywordCategoryHandler<KeywordCategory::ArrayKeywords>
{
    [[nodiscard]] static std::expected<void, QueryError>
    compile(CompileContext& ctx, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn& compileFn)
    {
        return compileArrayKeywords(ctx, schemaObj, node, compileFn);
    }
};

template <>
struct KeywordCategoryHandler<KeywordCategory::ObjectKeywords>
{
    [[nodiscard]] static std::expected<void, QueryError>
    compile(CompileContext& ctx, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn& compileFn)
    {
        return compileObjectKeywords(ctx, schemaObj, node, compileFn);
    }
};

template <>
struct KeywordCategoryHandler<KeywordCategory::Combinators>
{
    [[nodiscard]] static std::expected<void, QueryError>
    compile(CompileContext& ctx, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn& compileFn)
    {
        return compileCombinatorKeywords(ctx, schemaObj, node, compileFn);
    }
};

template <>
struct KeywordCategoryHandler<KeywordCategory::Metadata>
{
    [[nodiscard]] static std::expected<void, QueryError>
    compile(CompileContext&, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn&)
    {
        compileMetadataKeywords(schemaObj, node);
        return {};
    }
};

template <>
struct KeywordCategoryHandler<KeywordCategory::NestedDefs>
{
    [[nodiscard]] static std::expected<void, QueryError>
    compile(CompileContext& ctx, const QJsonObject& schemaObj, ObjectSchema&, CompileSchemaFn& compileFn)
    {
        return compileNestedDefs(ctx, schemaObj, compileFn);
    }
};

// ────────────────────────────────────────────────────────────────────────────
// Recursive Dispatch Table for Keyword Categories
// ────────────────────────────────────────────────────────────────────────────

template <KeywordCategory... Categories>
struct KeywordDispatchTable;

template <KeywordCategory First, KeywordCategory... Rest>
struct KeywordDispatchTable<First, Rest...>
{
    [[nodiscard]] static std::expected<void, QueryError>
    dispatch(CompileContext& ctx, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn& compileFn)
    {
        // Compile this category
        if (auto r{KeywordCategoryHandler<First>::compile(ctx, schemaObj, node, compileFn)}; !r)
            return r;

        // Continue with remaining categories
        return KeywordDispatchTable<Rest...>::dispatch(ctx, schemaObj, node, compileFn);
    }
};

// Base case: all categories processed
template <>
struct KeywordDispatchTable<>
{
    [[nodiscard]] static std::expected<void, QueryError>
    dispatch(CompileContext&, const QJsonObject&, ObjectSchema&, CompileSchemaFn&)
    {
        return {};
    }
};

// Full keyword dispatch chain
using FullKeywordDispatcher = KeywordDispatchTable<KeywordCategory::TypeConstraints,
                                                    KeywordCategory::StringKeywords,
                                                    KeywordCategory::NumericKeywords,
                                                    KeywordCategory::ArrayKeywords,
                                                    KeywordCategory::ObjectKeywords,
                                                    KeywordCategory::Combinators,
                                                    KeywordCategory::Metadata,
                                                    KeywordCategory::NestedDefs>;

} // namespace json_query::json_schema::internal
