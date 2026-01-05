// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "json-query/json-schema/JSONSchema.hpp"

using namespace json_query::json_schema;

class JSONSchemaRefTest : public ::testing::Test
{
  protected:
    static QJsonObject parseSchema(const char* json) { return QJsonDocument::fromJson(json).object(); }
};

// ============================================================================
// Basic $ref Tests
// ============================================================================

TEST_F(JSONSchemaRefTest, RefToDefinitions)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$defs": {
            "positiveInteger": {
                "type": "integer",
                "minimum": 1
            }
        },
        "type": "object",
        "properties": {
            "age": {"$ref": "#/$defs/positiveInteger"}
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"age"_qs] = 25;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalidNegative{};
    invalidNegative[u"age"_qs] = -5;
    EXPECT_FALSE(schemaResult->validate(invalidNegative).isValid());

    QJsonObject invalidType{};
    invalidType[u"age"_qs] = u"not a number"_qs;
    EXPECT_FALSE(schemaResult->validate(invalidType).isValid());
}

TEST_F(JSONSchemaRefTest, RefToRoot)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "array",
        "items": {"$ref": "#"}
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonArray valid{};
    QJsonArray nested{};
    nested.append(QJsonArray{});
    valid.append(nested);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonArray invalid{};
    invalid.append(42);
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaRefTest, RefToNestedDefinition)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$defs": {
            "address": {
                "type": "object",
                "properties": {
                    "street": {"type": "string"},
                    "city": {"type": "string"}
                },
                "required": ["street", "city"]
            }
        },
        "type": "object",
        "properties": {
            "billingAddress": {"$ref": "#/$defs/address"},
            "shippingAddress": {"$ref": "#/$defs/address"}
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject validAddress{};
    validAddress[u"street"_qs] = u"123 Main St"_qs;
    validAddress[u"city"_qs]   = u"Springfield"_qs;

    QJsonObject valid{};
    valid[u"billingAddress"_qs]  = validAddress;
    valid[u"shippingAddress"_qs] = validAddress;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalidAddress{};
    invalidAddress[u"street"_qs] = u"123 Main St"_qs;

    QJsonObject invalid{};
    invalid[u"billingAddress"_qs] = invalidAddress;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaRefTest, MultipleRefsToSameDefinition)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$defs": {
            "stringType": {"type": "string"}
        },
        "type": "object",
        "properties": {
            "firstName": {"$ref": "#/$defs/stringType"},
            "lastName": {"$ref": "#/$defs/stringType"},
            "middleName": {"$ref": "#/$defs/stringType"}
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"firstName"_qs]  = u"John"_qs;
    valid[u"lastName"_qs]   = u"Doe"_qs;
    valid[u"middleName"_qs] = u"Q"_qs;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"firstName"_qs] = u"John"_qs;
    invalid[u"lastName"_qs]  = 42;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

// ============================================================================
// $anchor Tests
// ============================================================================

TEST_F(JSONSchemaRefTest, AnchorReference)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$defs": {
            "positiveInteger": {
                "$anchor": "positiveInt",
                "type": "integer",
                "minimum": 1
            }
        },
        "type": "object",
        "properties": {
            "count": {"$ref": "#positiveInt"}
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"count"_qs] = 10;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"count"_qs] = -5;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

// ============================================================================
// Recursive Schema Tests
// ============================================================================

TEST_F(JSONSchemaRefTest, RecursiveSchema)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "children": {
                "type": "array",
                "items": {"$ref": "#"}
            }
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject child2{};
    child2[u"name"_qs] = u"Grandchild"_qs;

    QJsonArray  children{};
    QJsonObject child1{};
    child1[u"name"_qs] = u"Child"_qs;
    QJsonArray grandchildren{};
    grandchildren.append(child2);
    child1[u"children"_qs] = grandchildren;
    children.append(child1);

    QJsonObject root{};
    root[u"name"_qs]     = u"Root"_qs;
    root[u"children"_qs] = children;

    EXPECT_TRUE(schemaResult->validate(root).isValid());
}

TEST_F(JSONSchemaRefTest, MutuallyRecursiveSchemas)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$defs": {
            "node": {
                "type": "object",
                "properties": {
                    "value": {"type": "number"},
                    "next": {"$ref": "#/$defs/node"}
                }
            }
        },
        "$ref": "#/$defs/node"
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject node3{};
    node3[u"value"_qs] = 3;

    QJsonObject node2{};
    node2[u"value"_qs] = 2;
    node2[u"next"_qs]  = node3;

    QJsonObject node1{};
    node1[u"value"_qs] = 1;
    node1[u"next"_qs]  = node2;

    EXPECT_TRUE(schemaResult->validate(node1).isValid());

    QJsonObject invalidNode{};
    invalidNode[u"value"_qs] = u"not a number"_qs;
    EXPECT_FALSE(schemaResult->validate(invalidNode).isValid());
}

// ============================================================================
// Complex Reference Scenarios
// ============================================================================

TEST_F(JSONSchemaRefTest, RefInCombinators)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$defs": {
            "stringType": {"type": "string"},
            "numberType": {"type": "number"}
        },
        "anyOf": [
            {"$ref": "#/$defs/stringType"},
            {"$ref": "#/$defs/numberType"}
        ]
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(true)).isValid());
}

TEST_F(JSONSchemaRefTest, RefInConditional)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$defs": {
            "usAddress": {
                "properties": {
                    "zipCode": {"pattern": "^\\d{5}$"}
                }
            },
            "caAddress": {
                "properties": {
                    "postalCode": {"pattern": "^[A-Z]\\d[A-Z] \\d[A-Z]\\d$"}
                }
            }
        },
        "type": "object",
        "properties": {
            "country": {"type": "string"}
        },
        "if": {
            "properties": {"country": {"const": "US"}}
        },
        "then": {"$ref": "#/$defs/usAddress"},
        "else": {"$ref": "#/$defs/caAddress"}
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject usValid{};
    usValid[u"country"_qs] = u"US"_qs;
    usValid[u"zipCode"_qs] = u"12345"_qs;
    EXPECT_TRUE(schemaResult->validate(usValid).isValid());

    QJsonObject caValid{};
    caValid[u"country"_qs]    = u"CA"_qs;
    caValid[u"postalCode"_qs] = u"K1A 0B1"_qs;
    EXPECT_TRUE(schemaResult->validate(caValid).isValid());
}

TEST_F(JSONSchemaRefTest, NestedRefs)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$defs": {
            "name": {"type": "string"},
            "person": {
                "type": "object",
                "properties": {
                    "firstName": {"$ref": "#/$defs/name"},
                    "lastName": {"$ref": "#/$defs/name"}
                }
            }
        },
        "$ref": "#/$defs/person"
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"firstName"_qs] = u"John"_qs;
    valid[u"lastName"_qs]  = u"Doe"_qs;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"firstName"_qs] = 42;
    invalid[u"lastName"_qs]  = u"Doe"_qs;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(JSONSchemaRefTest, EmptyFragmentRefersToRoot)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "string",
        "properties": {
            "self": {"$ref": "#"}
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("test")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(42)).isValid());
}

TEST_F(JSONSchemaRefTest, RefWithAdditionalKeywords)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$defs": {
            "stringType": {"type": "string"}
        },
        "type": "object",
        "properties": {
            "name": {
                "$ref": "#/$defs/stringType",
                "minLength": 3
            }
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"name"_qs] = u"John"_qs;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject tooShort{};
    tooShort[u"name"_qs] = u"Jo"_qs;
    EXPECT_FALSE(schemaResult->validate(tooShort).isValid());
}
