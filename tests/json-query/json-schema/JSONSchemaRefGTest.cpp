// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "json-query/json-schema/JSONSchema.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

using namespace json_query::json_schema;
using json_query::literals::operator""_qt_s;

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
    valid[u"age"_qt_s] = 25;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalidNegative{};
    invalidNegative[u"age"_qt_s] = -5;
    EXPECT_FALSE(schemaResult->validate(invalidNegative).isValid());

    QJsonObject invalidType{};
    invalidType[u"age"_qt_s] = u"not a number"_qt_s;
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
    validAddress[u"street"_qt_s] = u"123 Main St"_qt_s;
    validAddress[u"city"_qt_s]   = u"Springfield"_qt_s;

    QJsonObject valid{};
    valid[u"billingAddress"_qt_s]  = validAddress;
    valid[u"shippingAddress"_qt_s] = validAddress;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalidAddress{};
    invalidAddress[u"street"_qt_s] = u"123 Main St"_qt_s;

    QJsonObject invalid{};
    invalid[u"billingAddress"_qt_s] = invalidAddress;
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
    valid[u"firstName"_qt_s]  = u"John"_qt_s;
    valid[u"lastName"_qt_s]   = u"Doe"_qt_s;
    valid[u"middleName"_qt_s] = u"Q"_qt_s;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"firstName"_qt_s] = u"John"_qt_s;
    invalid[u"lastName"_qt_s]  = 42;
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
    valid[u"count"_qt_s] = 10;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"count"_qt_s] = -5;
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
    child2[u"name"_qt_s] = u"Grandchild"_qt_s;

    QJsonArray  children{};
    QJsonObject child1{};
    child1[u"name"_qt_s] = u"Child"_qt_s;
    QJsonArray grandchildren{};
    grandchildren.append(child2);
    child1[u"children"_qt_s] = grandchildren;
    children.append(child1);

    QJsonObject root{};
    root[u"name"_qt_s]     = u"Root"_qt_s;
    root[u"children"_qt_s] = children;

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
    node3[u"value"_qt_s] = 3;

    QJsonObject node2{};
    node2[u"value"_qt_s] = 2;
    node2[u"next"_qt_s]  = node3;

    QJsonObject node1{};
    node1[u"value"_qt_s] = 1;
    node1[u"next"_qt_s]  = node2;

    EXPECT_TRUE(schemaResult->validate(node1).isValid());

    QJsonObject invalidNode{};
    invalidNode[u"value"_qt_s] = u"not a number"_qt_s;
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
    usValid[u"country"_qt_s] = u"US"_qt_s;
    usValid[u"zipCode"_qt_s] = u"12345"_qt_s;
    EXPECT_TRUE(schemaResult->validate(usValid).isValid());

    QJsonObject caValid{};
    caValid[u"country"_qt_s]    = u"CA"_qt_s;
    caValid[u"postalCode"_qt_s] = u"K1A 0B1"_qt_s;
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
    valid[u"firstName"_qt_s] = u"John"_qt_s;
    valid[u"lastName"_qt_s]  = u"Doe"_qt_s;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"firstName"_qt_s] = 42;
    invalid[u"lastName"_qt_s]  = u"Doe"_qt_s;
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
    valid[u"name"_qt_s] = u"John"_qt_s;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject tooShort{};
    tooShort[u"name"_qt_s] = u"Jo"_qt_s;
    EXPECT_FALSE(schemaResult->validate(tooShort).isValid());
}

// ============================================================================
// $ref Cycle Detection (unproductive recursion must not overflow the stack)
// ============================================================================

TEST_F(JSONSchemaRefTest, RootSelfRefDoesNotCrash)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"$ref": "#"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    // Must terminate (formerly stack overflow) and report the cycle
    auto result{schemaResult->validate(QJsonValue{42})};
    EXPECT_FALSE(result.isValid());
    ASSERT_FALSE(result.errors().empty());
    EXPECT_EQ(result.errors().front().code, json_query::Error{EvalError::RefCycleDetected});
}

TEST_F(JSONSchemaRefTest, MutualDefsCycleDoesNotCrash)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$defs": {
            "a": {"$ref": "#/$defs/b"},
            "b": {"$ref": "#/$defs/a"}
        },
        "$ref": "#/$defs/a"
    })"))};
    ASSERT_TRUE(schemaResult.has_value());
    EXPECT_FALSE(schemaResult->isValid(QJsonValue{42}));
}

TEST_F(JSONSchemaRefTest, ProductiveRecursionStillValidates)
{
    // Legal recursive schema: the $ref consumes instance depth via properties
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "object",
        "properties": {
            "child": {"$ref": "#"}
        },
        "additionalProperties": false
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    const auto nested{parseSchema(R"({"child": {"child": {"child": {}}}})")};
    EXPECT_TRUE(schemaResult->validate(nested).isValid());

    const auto bad{parseSchema(R"({"child": {"oops": 1}})")};
    EXPECT_FALSE(schemaResult->validate(bad).isValid());
}

// ============================================================================
// UnresolvedRefPolicy
// ============================================================================

TEST_F(JSONSchemaRefTest, UnresolvedRemoteRefAcceptsAllByDefault)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "$ref": "https://example.invalid/nope.json"
    })"))};
    ASSERT_TRUE(schemaResult.has_value());
    // Spec-lenient default: unresolved subschema accepts anything
    EXPECT_TRUE(schemaResult->isValid(QJsonValue{42}));
    EXPECT_TRUE(schemaResult->isValid(QJsonValue{u"str"_qt_s}));
}

TEST_F(JSONSchemaRefTest, UnresolvedRemoteRefFailsWithFailPolicy)
{
    SchemaOptions options{};
    options.unresolvedRefPolicy = UnresolvedRefPolicy::Fail;

    auto schemaResult{JSONSchema::create(QJsonValue{parseSchema(R"({
        "$ref": "https://example.invalid/nope.json"
    })")},
                                         nullptr,
                                         options)};
    ASSERT_FALSE(schemaResult.has_value());
    EXPECT_EQ(schemaResult.error(), json_query::Error{ParseError::UnresolvedReference});
}

TEST_F(JSONSchemaRefTest, ResolvedRefsPassWithFailPolicy)
{
    SchemaOptions options{};
    options.unresolvedRefPolicy = UnresolvedRefPolicy::Fail;

    auto schemaResult{JSONSchema::create(QJsonValue{parseSchema(R"({
        "$defs": {"s": {"type": "string"}},
        "$ref": "#/$defs/s"
    })")},
                                         nullptr,
                                         options)};
    ASSERT_TRUE(schemaResult.has_value());
    EXPECT_TRUE(schemaResult->isValid(QJsonValue{u"ok"_qt_s}));
    EXPECT_FALSE(schemaResult->isValid(QJsonValue{42}));
}
