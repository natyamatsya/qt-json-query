// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "json-query/json-schema/JSONSchema.hpp"

using namespace json_query::json_schema;

class JSONSchemaBasicTest : public ::testing::Test
{
  protected:
    static QJsonObject parseSchema(const char* json)
    {
        return QJsonDocument::fromJson(json).object();
    }
};

// ============================================================================
// Boolean Schemas
// ============================================================================

TEST_F(JSONSchemaBasicTest, TrueSchemaAcceptsAnything)
{
    auto schemaResult = JSONSchema::create(QJsonValue(true));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue()).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(true)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonObject{})).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{})).isValid());
}

TEST_F(JSONSchemaBasicTest, FalseSchemaRejectsEverything)
{
    auto schemaResult = JSONSchema::create(QJsonValue(false));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_FALSE(schemaResult->validate(QJsonValue()).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(true)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonObject{})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonArray{})).isValid());
}

TEST_F(JSONSchemaBasicTest, EmptySchemaAcceptsAnything)
{
    auto schemaResult = JSONSchema::create(QJsonObject{});
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue()).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
}

// ============================================================================
// Type Keyword
// ============================================================================

TEST_F(JSONSchemaBasicTest, TypeString)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "string"})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(true)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue()).isValid());
}

TEST_F(JSONSchemaBasicTest, TypeNumber)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "number"})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(3.14)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(-100)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("42")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(true)).isValid());
}

TEST_F(JSONSchemaBasicTest, TypeInteger)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "integer"})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(-100)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(3.14)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("42")).isValid());
}

TEST_F(JSONSchemaBasicTest, TypeBoolean)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "boolean"})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(true)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(false)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(1)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("true")).isValid());
}

TEST_F(JSONSchemaBasicTest, TypeNull)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "null"})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue()).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(false)).isValid());
}

TEST_F(JSONSchemaBasicTest, TypeArray)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "array"})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{})).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{1, 2, 3})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonObject{})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("[]")).isValid());
}

TEST_F(JSONSchemaBasicTest, TypeObject)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "object"})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonObject{})).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonObject{{"a", 1}})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonArray{})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("{}")).isValid());
}

TEST_F(JSONSchemaBasicTest, TypeMultiple)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": ["string", "number"]})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(true)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonObject{})).isValid());
}

// ============================================================================
// Enum Keyword
// ============================================================================

TEST_F(JSONSchemaBasicTest, EnumStrings)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"enum": ["red", "green", "blue"]})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("red")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("green")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("blue")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("yellow")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(1)).isValid());
}

TEST_F(JSONSchemaBasicTest, EnumMixedTypes)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"enum": ["yes", 1, true, null]})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("yes")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(1)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(true)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue()).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("no")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(2)).isValid());
}

// ============================================================================
// Const Keyword
// ============================================================================

TEST_F(JSONSchemaBasicTest, ConstString)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"const": "hello"})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("world")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(42)).isValid());
}

TEST_F(JSONSchemaBasicTest, ConstNumber)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"const": 42})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(43)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("42")).isValid());
}

TEST_F(JSONSchemaBasicTest, ConstObject)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"const": {"a": 1, "b": 2}})"));
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject matching{{"a", 1}, {"b", 2}};
    QJsonObject different{{"a", 1}, {"b", 3}};

    EXPECT_TRUE(schemaResult->validate(QJsonValue(matching)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(different)).isValid());
}

// ============================================================================
// String Keywords
// ============================================================================

TEST_F(JSONSchemaBasicTest, MinLength)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "string", "minLength": 3})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("abc")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("abcd")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("ab")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("")).isValid());
}

TEST_F(JSONSchemaBasicTest, MaxLength)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "string", "maxLength": 5})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("hello!")).isValid());
}

TEST_F(JSONSchemaBasicTest, Pattern)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "string", "pattern": "^[a-z]+$"})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("abc")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("ABC")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("abc123")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("")).isValid());
}

// ============================================================================
// Numeric Keywords
// ============================================================================

TEST_F(JSONSchemaBasicTest, Minimum)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "number", "minimum": 0})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(100)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(-1)).isValid());
}

TEST_F(JSONSchemaBasicTest, Maximum)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "number", "maximum": 100})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(100)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(101)).isValid());
}

TEST_F(JSONSchemaBasicTest, ExclusiveMinimum)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "number", "exclusiveMinimum": 0})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(1)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(0.001)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(-1)).isValid());
}

TEST_F(JSONSchemaBasicTest, ExclusiveMaximum)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "number", "exclusiveMaximum": 100})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(99)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(99.999)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(100)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(101)).isValid());
}

TEST_F(JSONSchemaBasicTest, MultipleOf)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "number", "multipleOf": 5})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(5)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(10)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(-15)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(3)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(7)).isValid());
}

// ============================================================================
// Array Keywords
// ============================================================================

TEST_F(JSONSchemaBasicTest, MinItems)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "array", "minItems": 2})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{1, 2})).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{1, 2, 3})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonArray{1})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonArray{})).isValid());
}

TEST_F(JSONSchemaBasicTest, MaxItems)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "array", "maxItems": 3})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{})).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{1, 2, 3})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonArray{1, 2, 3, 4})).isValid());
}

TEST_F(JSONSchemaBasicTest, UniqueItems)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "array", "uniqueItems": true})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{1, 2, 3})).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{"a", "b", "c"})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonArray{1, 2, 1})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonArray{"a", "a"})).isValid());
}

// ============================================================================
// Object Keywords
// ============================================================================

TEST_F(JSONSchemaBasicTest, Required)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "type": "object",
        "required": ["name", "age"]
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{{"name", "Alice"}, {"age", 30}};
    QJsonObject missingAge{{"name", "Alice"}};
    QJsonObject missingName{{"age", 30}};

    EXPECT_TRUE(schemaResult->validate(QJsonValue(valid)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(missingAge)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(missingName)).isValid());
}

TEST_F(JSONSchemaBasicTest, Properties)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "age": {"type": "integer"}
        }
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{{"name", "Alice"}, {"age", 30}};
    QJsonObject wrongType{{"name", 123}, {"age", 30}};

    EXPECT_TRUE(schemaResult->validate(QJsonValue(valid)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(wrongType)).isValid());
}

TEST_F(JSONSchemaBasicTest, AdditionalPropertiesFalse)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "type": "object",
        "properties": {
            "name": {"type": "string"}
        },
        "additionalProperties": false
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{{"name", "Alice"}};
    QJsonObject extra{{"name", "Alice"}, {"age", 30}};

    EXPECT_TRUE(schemaResult->validate(QJsonValue(valid)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(extra)).isValid());
}

TEST_F(JSONSchemaBasicTest, MinMaxProperties)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "type": "object",
        "minProperties": 1,
        "maxProperties": 3
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject one{{"a", 1}};
    QJsonObject three{{"a", 1}, {"b", 2}, {"c", 3}};
    QJsonObject empty{};
    QJsonObject four{{"a", 1}, {"b", 2}, {"c", 3}, {"d", 4}};

    EXPECT_TRUE(schemaResult->validate(QJsonValue(one)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(three)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(empty)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(four)).isValid());
}

// ============================================================================
// Combinators
// ============================================================================

TEST_F(JSONSchemaBasicTest, AllOf)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "allOf": [
            {"type": "number"},
            {"minimum": 0},
            {"maximum": 100}
        ]
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(50)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(100)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(-1)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(101)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("50")).isValid());
}

TEST_F(JSONSchemaBasicTest, AnyOf)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "anyOf": [
            {"type": "string"},
            {"type": "number"}
        ]
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(true)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonArray{})).isValid());
}

TEST_F(JSONSchemaBasicTest, OneOf)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "oneOf": [
            {"type": "integer", "multipleOf": 2},
            {"type": "integer", "multipleOf": 3}
        ]
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(2)).isValid());   // 2 only
    EXPECT_TRUE(schemaResult->validate(QJsonValue(3)).isValid());   // 3 only
    EXPECT_TRUE(schemaResult->validate(QJsonValue(4)).isValid());   // 2 only
    EXPECT_FALSE(schemaResult->validate(QJsonValue(6)).isValid());  // Both 2 and 3
    EXPECT_FALSE(schemaResult->validate(QJsonValue(1)).isValid());  // Neither
}

TEST_F(JSONSchemaBasicTest, Not)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "not": {"type": "string"}
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(true)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{})).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("hello")).isValid());
}

// ============================================================================
// If/Then/Else
// ============================================================================

TEST_F(JSONSchemaBasicTest, IfThenElse)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "if": {"type": "string"},
        "then": {"minLength": 3},
        "else": {"minimum": 0}
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    // Strings must have minLength 3
    EXPECT_TRUE(schemaResult->validate(QJsonValue("abc")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("ab")).isValid());

    // Numbers must be >= 0
    EXPECT_TRUE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(100)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(-1)).isValid());
}

// ============================================================================
// Error Reporting
// ============================================================================

TEST_F(JSONSchemaBasicTest, ErrorContainsLocation)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "type": "object",
        "properties": {
            "name": {"type": "string"}
        }
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject invalid{{"name", 123}};
    auto        result = schemaResult->validate(QJsonValue(invalid));

    ASSERT_FALSE(result.isValid());
    ASSERT_EQ(result.errorCount(), 1u);

    const auto& error = result.errors().front();
    EXPECT_EQ(error.instanceLocation, u"/name");
    EXPECT_FALSE(error.message.isEmpty());
}

TEST_F(JSONSchemaBasicTest, MultipleErrors)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "age": {"type": "integer"}
        },
        "required": ["name", "age"]
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject invalid{{"name", 123}};  // wrong type and missing age
    auto        result = schemaResult->validate(QJsonValue(invalid));

    ASSERT_FALSE(result.isValid());
    EXPECT_GE(result.errorCount(), 2u);  // At least type error + required error
}

// ============================================================================
// Schema Metadata
// ============================================================================

TEST_F(JSONSchemaBasicTest, SchemaIdAndDialect)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({
        "$id": "https://example.com/my-schema",
        "$schema": "https://json-schema.org/draft/2020-12/schema",
        "type": "object"
    })"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_EQ(schemaResult->schemaId(), u"https://example.com/my-schema");
    EXPECT_EQ(schemaResult->schemaVersion(), u"https://json-schema.org/draft/2020-12/schema");
}

// ============================================================================
// Quick Validation (isValid)
// ============================================================================

TEST_F(JSONSchemaBasicTest, IsValidQuickCheck)
{
    auto schemaResult = JSONSchema::create(parseSchema(R"({"type": "string"})"));
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->isValid(QJsonValue("hello")));
    EXPECT_FALSE(schemaResult->isValid(QJsonValue(42)));
}
