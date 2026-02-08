// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>
#include <QtCore/QRegularExpression>
#include <QtCore/QString>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

namespace json_query::json_schema::internal
{

/**
 * @brief JSON Schema type enumeration matching the spec
 */
enum class SchemaType : std::uint8_t
{
    Null    = 0,
    Boolean = 1,
    Integer = 2,
    Number  = 3,
    String  = 4,
    Array   = 5,
    Object  = 6,
};

/**
 * @brief Type constraint - can be single type or multiple types
 */
struct TypeConstraint
{
    std::vector<SchemaType> allowedTypes{};

    [[nodiscard]] bool allows(SchemaType t) const noexcept
    {
        for (auto allowed : allowedTypes)
        {
            if (allowed == t)
                return true;
            // integer is also a valid number
            if (allowed == SchemaType::Number && t == SchemaType::Integer)
                return true;
        }
        return false;
    }

    [[nodiscard]] bool isSingleType() const noexcept { return allowedTypes.size() == 1; }
};

/**
 * @brief Boolean schema (true = accept all, false = reject all)
 */
struct BooleanSchema
{
    bool value; // true = always valid, false = always invalid
};

/**
 * @brief Reference to another schema (resolved at compile time)
 */
struct RefSchema
{
    static constexpr std::size_t kUnresolved{std::numeric_limits<std::size_t>::max()};
    static constexpr std::size_t kNoIndex{std::numeric_limits<std::size_t>::max()};

    std::size_t targetIndex{kUnresolved}; // Index into CompiledSchema::nodes
    QString     originalRef;              // Original $ref string for error messages
    std::size_t selfIndex{kNoIndex};      // Own node index for resource scope push
    QString     baseUri;                  // Base URI at compile time for relative ref resolution

    [[nodiscard]] bool isResolved() const noexcept { return targetIndex != kUnresolved; }
};

/**
 * @brief Full object schema with all keywords
 *
 * Uses indices into the CompiledSchema::nodes vector for sub-schemas.
 * This flat structure is cache-friendly and avoids pointer chasing.
 */
struct ObjectSchema
{
    // Type constraints
    std::optional<TypeConstraint> type;

    // Value constraints
    std::optional<QJsonArray> enumValues;
    std::optional<QJsonValue> constValue;

    // Object keywords
    std::unordered_map<QString, std::size_t>                properties; // property name → node index
    std::vector<std::pair<QRegularExpression, std::size_t>> patternProperties;
    std::optional<std::size_t>                              additionalProperties;
    std::vector<QString>                                    required{};
    std::optional<std::size_t>                              minProperties;
    std::optional<std::size_t>                              maxProperties;
    std::optional<std::size_t>                              propertyNames; // schema for property names

    // Array keywords
    std::optional<std::size_t> items;       // node index for items schema (2020-12 style)
    std::vector<std::size_t>   prefixItems; // node indices for prefix items
    std::optional<std::size_t> contains;
    std::optional<std::size_t> minContains;
    std::optional<std::size_t> maxContains;
    std::optional<std::size_t> minItems;
    std::optional<std::size_t> maxItems;
    bool                       uniqueItems = false;

    // String keywords
    std::optional<QRegularExpression> pattern;
    std::optional<std::size_t>        minLength;
    std::optional<std::size_t>        maxLength;
    std::optional<QString>            format;

    // Numeric keywords
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> exclusiveMinimum;
    std::optional<double> exclusiveMaximum;
    std::optional<double> multipleOf;

    // Combinators (store node indices)
    std::vector<std::size_t>   allOf{};
    std::vector<std::size_t>   anyOf{};
    std::vector<std::size_t>   oneOf{};
    std::optional<std::size_t> notSchema;

    // Conditional
    std::optional<std::size_t> ifSchema;
    std::optional<std::size_t> thenSchema;
    std::optional<std::size_t> elseSchema;

    // Dependencies (2019-09+)
    std::unordered_map<QString, std::vector<QString>> dependentRequired;
    std::unordered_map<QString, std::size_t>          dependentSchemas;

    // Unevaluated (2019-09+)
    std::optional<std::size_t> unevaluatedProperties;
    std::optional<std::size_t> unevaluatedItems;

    // Node identity (for resource scope management during validation)
    static constexpr std::size_t kNoIndex{std::numeric_limits<std::size_t>::max()};
    std::size_t selfIndex{kNoIndex};

    // Metadata (not used for validation, but stored for introspection)
    std::optional<QString> title;
    std::optional<QString> description;
};

/**
 * @brief Dynamic reference to another schema (resolved at validation time)
 *
 * Like RefSchema, but walks the dynamic scope chain at validation time
 * to find the outermost $dynamicAnchor with the matching name.
 */
struct DynamicRefSchema
{
    static constexpr std::size_t kUnresolved{std::numeric_limits<std::size_t>::max()};

    std::size_t targetIndex{kUnresolved}; // Static fallback target
    QString     anchorName;              // Dynamic anchor name to search for at runtime
    std::size_t selfIndex{kUnresolved};  // Own node index for resource scope push
    QString     originalRef;             // Original $dynamicRef string for error messages
    QString     baseUri;                 // Base URI at compile time for relative ref resolution

    [[nodiscard]] bool isResolved() const noexcept { return targetIndex != kUnresolved; }
};

/**
 * @brief Variant type for all schema node types
 */
using SchemaNode = std::variant<BooleanSchema, ObjectSchema, RefSchema, DynamicRefSchema>;

/**
 * @brief Compiled schema - flat array of nodes for cache-friendly traversal
 */
struct CompiledSchema
{
    std::vector<SchemaNode> nodes{};
    std::size_t             rootIndex = 0;

    // Anchor registry for $ref resolution
    std::unordered_map<QString, std::size_t> anchors;        // $anchor → node index
    std::unordered_map<QString, std::size_t> dynamicAnchors; // $dynamicAnchor → node index (flat)

    // Per-resource dynamic anchor maps for $dynamicRef scope resolution
    // Maps resource root node index → {anchor name → target node index}
    std::unordered_map<std::size_t, std::unordered_map<QString, std::size_t>> resourceDynamicAnchors;

    // Per-node $dynamicAnchor name for bookending check
    // Maps node index → anchor name (only for nodes that have $dynamicAnchor)
    std::unordered_map<std::size_t, QString> nodeDynAnchorNames;

    // Schema metadata
    QString schemaId; // $id if present
    QString dialect;  // $schema if present

    /**
     * @brief Get the root schema node
     */
    [[nodiscard]] const SchemaNode& root() const { return nodes.at(rootIndex); }

    /**
     * @brief Get a node by index
     */
    [[nodiscard]] const SchemaNode& nodeAt(std::size_t index) const { return nodes.at(index); }
};

/**
 * @brief Map QJsonValue type to SchemaType
 */
[[nodiscard]] inline SchemaType jsonValueToSchemaType(const QJsonValue& value) noexcept
{
    switch (value.type())
    {
    case QJsonValue::Null:
        return SchemaType::Null;
    case QJsonValue::Bool:
        return SchemaType::Boolean;
    case QJsonValue::Double:
    {
        // Check if it's an integer
        const auto d{value.toDouble()};
        if (d == static_cast<double>(static_cast<qint64>(d)))
            return SchemaType::Integer;
        return SchemaType::Number;
    }
    case QJsonValue::String:
        return SchemaType::String;
    case QJsonValue::Array:
        return SchemaType::Array;
    case QJsonValue::Object:
        return SchemaType::Object;
    default:
        return SchemaType::Null;
    }
}

/**
 * @brief Convert type string to SchemaType
 */
[[nodiscard]] inline std::optional<SchemaType> stringToSchemaType(QStringView typeStr) noexcept
{
    if (typeStr == u"null")
        return SchemaType::Null;
    if (typeStr == u"boolean")
        return SchemaType::Boolean;
    if (typeStr == u"integer")
        return SchemaType::Integer;
    if (typeStr == u"number")
        return SchemaType::Number;
    if (typeStr == u"string")
        return SchemaType::String;
    if (typeStr == u"array")
        return SchemaType::Array;
    if (typeStr == u"object")
        return SchemaType::Object;
    return std::nullopt;
}

/**
 * @brief Convert SchemaType to string for error messages
 */
[[nodiscard]] inline QStringView schemaTypeToString(SchemaType type) noexcept
{
    switch (type)
    {
    case SchemaType::Null:
        return u"null";
    case SchemaType::Boolean:
        return u"boolean";
    case SchemaType::Integer:
        return u"integer";
    case SchemaType::Number:
        return u"number";
    case SchemaType::String:
        return u"string";
    case SchemaType::Array:
        return u"array";
    case SchemaType::Object:
        return u"object";
    default:
        return u"unknown";
    }
}

} // namespace json_query::json_schema::internal
