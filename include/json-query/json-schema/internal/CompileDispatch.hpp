// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/CompileContext.hpp"
#include "json-query/json-schema/internal/CompileKeywords.hpp"
#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

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
[[nodiscard]] inline std::expected<void, QueryError> compileStringKeywords(const QJsonObject& schemaObj,
                                                                           ObjectSchema&      node)
{
    using json_query::literals::operator""_qt_s;

    if (auto r{parsePatternKeyword(schemaObj[u"pattern"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.pattern = std::move(*r);

    if (auto r{parseIntegerKeyword(schemaObj[u"minLength"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.minLength = *r;

    if (auto r{parseIntegerKeyword(schemaObj[u"maxLength"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.maxLength = *r;

    // Format is just a string, no parsing needed
    if (schemaObj.contains(u"format"_qt_s) && schemaObj[u"format"_qt_s].isString())
        node.format = schemaObj[u"format"_qt_s].toString();

    return {};
}

/**
 * @brief Compile numeric-related keywords (minimum, maximum, exclusiveMinimum, exclusiveMaximum, multipleOf)
 */
[[nodiscard]] inline std::expected<void, QueryError> compileNumericKeywords(const QJsonObject& schemaObj,
                                                                            ObjectSchema&      node)
{
    using json_query::literals::operator""_qt_s;

    if (auto r{parseNumericKeyword(schemaObj[u"minimum"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.minimum = *r;

    if (auto r{parseNumericKeyword(schemaObj[u"maximum"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.maximum = *r;

    if (auto r{parseNumericKeyword(schemaObj[u"exclusiveMinimum"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.exclusiveMinimum = *r;

    if (auto r{parseNumericKeyword(schemaObj[u"exclusiveMaximum"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.exclusiveMaximum = *r;

    if (auto r{parseNumericKeyword(schemaObj[u"multipleOf"_qt_s])}; !r)
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
    using json_query::literals::operator""_qt_s;

    if (auto r{parseIntegerKeyword(schemaObj[u"minItems"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.minItems = *r;

    if (auto r{parseIntegerKeyword(schemaObj[u"maxItems"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.maxItems = *r;

    if (schemaObj.contains(u"uniqueItems"_qt_s))
    {
        const auto uniqueVal{schemaObj[u"uniqueItems"_qt_s]};
        if (uniqueVal.isBool())
            node.uniqueItems = uniqueVal.toBool();
    }

    // prefixItems - array of schemas
    if (schemaObj.contains(u"prefixItems"_qt_s) && schemaObj[u"prefixItems"_qt_s].isArray())
    {
        for (const auto& item : schemaObj[u"prefixItems"_qt_s].toArray())
        {
            auto r{compile(ctx, item)};
            if (!r)
                return std::unexpected(r.error());
            node.prefixItems.push_back(*r);
        }
    }

    // items - single schema for additional items
    if (schemaObj.contains(u"items"_qt_s))
    {
        auto r{compile(ctx, schemaObj[u"items"_qt_s])};
        if (!r)
            return std::unexpected(r.error());
        node.items = *r;
    }

    // contains
    if (schemaObj.contains(u"contains"_qt_s))
    {
        auto r{compile(ctx, schemaObj[u"contains"_qt_s])};
        if (!r)
            return std::unexpected(r.error());
        node.contains = *r;
    }

    // minContains / maxContains (only meaningful when contains is present)
    if (auto r{parseIntegerKeyword(schemaObj[u"minContains"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.minContains = *r;

    if (auto r{parseIntegerKeyword(schemaObj[u"maxContains"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.maxContains = *r;

    // unevaluatedItems
    if (schemaObj.contains(u"unevaluatedItems"_qt_s))
    {
        auto r{compile(ctx, schemaObj[u"unevaluatedItems"_qt_s])};
        if (!r)
            return std::unexpected(r.error());
        node.unevaluatedItems = *r;
    }

    return {};
}

/**
 * @brief Compile object-related keywords (properties, required, additionalProperties, etc.)
 */
[[nodiscard]] inline std::expected<void, QueryError>
compileObjectKeywords(CompileContext& ctx, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn& compile)
{
    using json_query::literals::operator""_qt_s;

    // required
    if (auto r{parseRequiredKeyword(schemaObj[u"required"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.required = std::move(*r);

    // minProperties, maxProperties
    if (auto r{parseIntegerKeyword(schemaObj[u"minProperties"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.minProperties = *r;

    if (auto r{parseIntegerKeyword(schemaObj[u"maxProperties"_qt_s])}; !r)
        return std::unexpected(r.error());
    else
        node.maxProperties = *r;

    // properties - map of property schemas
    if (schemaObj.contains(u"properties"_qt_s) && schemaObj[u"properties"_qt_s].isObject())
    {
        const auto propsObj{schemaObj[u"properties"_qt_s].toObject()};
        for (auto it = propsObj.begin(); it != propsObj.end(); ++it)
        {
            auto r{compile(ctx, it.value())};
            if (!r)
                return std::unexpected(r.error());
            node.properties[it.key()] = *r;
        }
    }

    // additionalProperties
    if (schemaObj.contains(u"additionalProperties"_qt_s))
    {
        auto r{compile(ctx, schemaObj[u"additionalProperties"_qt_s])};
        if (!r)
            return std::unexpected(r.error());
        node.additionalProperties = *r;
    }

    // patternProperties
    if (schemaObj.contains(u"patternProperties"_qt_s) && schemaObj[u"patternProperties"_qt_s].isObject())
    {
        const auto patternPropsObj{schemaObj[u"patternProperties"_qt_s].toObject()};
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
    if (schemaObj.contains(u"propertyNames"_qt_s))
    {
        auto r{compile(ctx, schemaObj[u"propertyNames"_qt_s])};
        if (!r)
            return std::unexpected(r.error());
        node.propertyNames = *r;
    }

    // dependentRequired
    if (schemaObj.contains(u"dependentRequired"_qt_s) && schemaObj[u"dependentRequired"_qt_s].isObject())
    {
        const auto depReqObj{schemaObj[u"dependentRequired"_qt_s].toObject()};
        for (auto it = depReqObj.begin(); it != depReqObj.end(); ++it)
        {
            if (it.value().isArray())
            {
                std::vector<QString> requiredProps{};
                for (const QJsonValue& req : it.value().toArray())
                    if (req.isString())
                        requiredProps.push_back(req.toString());
                node.dependentRequired[it.key()] = std::move(requiredProps);
            }
        }
    }

    // dependentSchemas
    if (schemaObj.contains(u"dependentSchemas"_qt_s) && schemaObj[u"dependentSchemas"_qt_s].isObject())
    {
        const auto depSchObj{schemaObj[u"dependentSchemas"_qt_s].toObject()};
        for (auto it = depSchObj.begin(); it != depSchObj.end(); ++it)
        {
            auto r{compile(ctx, it.value())};
            if (!r)
                return std::unexpected(r.error());
            node.dependentSchemas[it.key()] = *r;
        }
    }

    // Legacy dependencies keyword (splits into dependentRequired + dependentSchemas)
    if (schemaObj.contains(u"dependencies"_qt_s) && schemaObj[u"dependencies"_qt_s].isObject())
    {
        const auto depsObj{schemaObj[u"dependencies"_qt_s].toObject()};
        for (auto it = depsObj.begin(); it != depsObj.end(); ++it)
        {
            if (it.value().isArray())
            {
                // Array value → dependentRequired
                std::vector<QString> requiredProps{};
                for (const QJsonValue& req : it.value().toArray())
                    if (req.isString())
                        requiredProps.push_back(req.toString());
                node.dependentRequired[it.key()] = std::move(requiredProps);
            }
            else
            {
                // Schema value → dependentSchemas
                auto r{compile(ctx, it.value())};
                if (!r)
                    return std::unexpected(r.error());
                node.dependentSchemas[it.key()] = *r;
            }
        }
    }

    // unevaluatedProperties
    if (schemaObj.contains(u"unevaluatedProperties"_qt_s))
    {
        auto r{compile(ctx, schemaObj[u"unevaluatedProperties"_qt_s])};
        if (!r)
            return std::unexpected(r.error());
        node.unevaluatedProperties = *r;
    }

    return {};
}

/**
 * @brief Compile combinator keywords (allOf, anyOf, oneOf, not, if/then/else)
 */
[[nodiscard]] inline std::expected<void, QueryError> compileCombinatorKeywords(CompileContext&    ctx,
                                                                               const QJsonObject& schemaObj,
                                                                               ObjectSchema&      node,
                                                                               CompileSchemaFn&   compile)
{
    using json_query::literals::operator""_qt_s;

    // allOf
    if (schemaObj.contains(u"allOf"_qt_s) && schemaObj[u"allOf"_qt_s].isArray())
    {
        for (const auto& item : schemaObj[u"allOf"_qt_s].toArray())
        {
            auto r{compile(ctx, item)};
            if (!r)
                return std::unexpected(r.error());
            node.allOf.push_back(*r);
        }
    }

    // anyOf
    if (schemaObj.contains(u"anyOf"_qt_s) && schemaObj[u"anyOf"_qt_s].isArray())
    {
        for (const auto& item : schemaObj[u"anyOf"_qt_s].toArray())
        {
            auto r{compile(ctx, item)};
            if (!r)
                return std::unexpected(r.error());
            node.anyOf.push_back(*r);
        }
    }

    // oneOf
    if (schemaObj.contains(u"oneOf"_qt_s) && schemaObj[u"oneOf"_qt_s].isArray())
    {
        for (const auto& item : schemaObj[u"oneOf"_qt_s].toArray())
        {
            auto r{compile(ctx, item)};
            if (!r)
                return std::unexpected(r.error());
            node.oneOf.push_back(*r);
        }
    }

    // not
    if (schemaObj.contains(u"not"_qt_s))
    {
        auto r{compile(ctx, schemaObj[u"not"_qt_s])};
        if (!r)
            return std::unexpected(r.error());
        node.notSchema = *r;
    }

    // if/then/else
    if (schemaObj.contains(u"if"_qt_s))
    {
        auto r{compile(ctx, schemaObj[u"if"_qt_s])};
        if (!r)
            return std::unexpected(r.error());
        node.ifSchema = *r;
    }

    if (schemaObj.contains(u"then"_qt_s))
    {
        auto r{compile(ctx, schemaObj[u"then"_qt_s])};
        if (!r)
            return std::unexpected(r.error());
        node.thenSchema = *r;
    }

    if (schemaObj.contains(u"else"_qt_s))
    {
        auto r{compile(ctx, schemaObj[u"else"_qt_s])};
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
    using json_query::literals::operator""_qt_s;

    if (schemaObj.contains(u"title"_qt_s) && schemaObj[u"title"_qt_s].isString())
        node.title = schemaObj[u"title"_qt_s].toString();

    if (schemaObj.contains(u"description"_qt_s) && schemaObj[u"description"_qt_s].isString())
        node.description = schemaObj[u"description"_qt_s].toString();
}

/**
 * @brief Process nested $defs for anchor resolution
 *
 * **Design Note: Why is this a no-op?**
 *
 * This function is intentionally empty because $defs must be handled at the
 * root level using a two-pass compilation approach (see compileSchema()).
 *
 * **The Problem:**
 * If we compile $defs recursively through the dispatcher, we get infinite
 * recursion when definitions reference each other:
 *
 *   $defs: {
 *     "A": { "$ref": "#/$defs/B" },  // A references B
 *     "B": { "$ref": "#/$defs/A" }   // B references A
 *   }
 *
 * Compiling A triggers compilation of B, which triggers compilation of A again.
 *
 * **The Solution:**
 * Use a two-pass approach (standard in compiler theory):
 * 1. Pass 1: Register all $defs in the anchor table (symbol table construction)
 * 2. Pass 2: Compile schemas recursively (code generation)
 *
 * This ensures all symbols are registered before any schema bodies are compiled,
 * preventing infinite recursion and allowing forward references.
 *
 * This placeholder exists to maintain the dispatch table structure and document
 * why $defs are NOT processed during recursive schema compilation.
 */
[[nodiscard]] inline std::expected<void, QueryError>
compileNestedDefs(CompileContext&, const QJsonObject&, CompileSchemaFn&)
{
    // $defs are compiled at root level in Phase 1 (symbol table construction)
    // before Phase 2 (recursive schema compilation) to prevent infinite recursion.
    // Nested $defs inside $ref-only schemas are handled by compileSchemaNode's fast path.
    return {};
}

// ────────────────────────────────────────────────────────────────────────────
// TableGen-Inspired Keyword Category Dispatch
// ────────────────────────────────────────────────────────────────────────────

enum class KeywordCategory
{
    TypeConstraints, // type, enum, const
    StringKeywords,  // pattern, minLength, maxLength, format
    NumericKeywords, // minimum, maximum, exclusiveMinimum, exclusiveMaximum, multipleOf
    ArrayKeywords,   // minItems, maxItems, uniqueItems, prefixItems, items, contains
    ObjectKeywords,  // properties, required, additionalProperties, patternProperties, etc.
    Combinators,     // allOf, anyOf, oneOf, not, if/then/else
    Metadata,        // title, description
    NestedDefs       // $defs processing
};

template <KeywordCategory Category>
struct KeywordCategoryHandler;

template <>
struct KeywordCategoryHandler<KeywordCategory::TypeConstraints>
{
    [[nodiscard]] static std::expected<void, QueryError>
    compile(CompileContext&, const QJsonObject& schemaObj, ObjectSchema& node, CompileSchemaFn&)
    {
        using json_query::literals::operator""_qt_s;

        if (auto r{parseTypeKeyword(schemaObj[u"type"_qt_s])}; !r)
            return std::unexpected(r.error());
        else
            node.type = *r;

        if (auto r{parseEnumKeyword(schemaObj[u"enum"_qt_s])}; !r)
            return std::unexpected(r.error());
        else
            node.enumValues = *r;

        node.constValue = parseConstKeyword(schemaObj[u"const"_qt_s]);
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
