// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "json-query/json-schema/JSONSchema.hpp"

using namespace json_query::json_schema;

class JSONSchemaKeywordTest : public ::testing::Test
{
  protected:
    static QJsonObject parseSchema(const char* json) { return QJsonDocument::fromJson(json).object(); }
};

// ============================================================================
// Combinator Tests
// ============================================================================

TEST_F(JSONSchemaKeywordTest, AllOfSuccess)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "allOf": [
            {"type": "string"},
            {"minLength": 5}
        ]
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello world")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("hi")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(42)).isValid());
}

TEST_F(JSONSchemaKeywordTest, AnyOfSuccess)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "anyOf": [
            {"type": "string"},
            {"type": "number"}
        ]
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(3.14)).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue(true)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(QJsonArray{})).isValid());
}

TEST_F(JSONSchemaKeywordTest, OneOfSuccess)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "oneOf": [
            {"type": "string", "maxLength": 5},
            {"type": "string", "minLength": 10}
        ]
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("hi")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello world")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("medium")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(42)).isValid());
}

TEST_F(JSONSchemaKeywordTest, NotSuccess)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "not": {"type": "string"}
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(true)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(QJsonArray{})).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("hello")).isValid());
}

TEST_F(JSONSchemaKeywordTest, IfThenElseSuccess)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "object",
        "if": {
            "properties": {"country": {"const": "USA"}}
        },
        "then": {
            "properties": {"postalCode": {"pattern": "^\\d{5}$"}}
        },
        "else": {
            "properties": {"postalCode": {"type": "string"}}
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject usaValid{};
    usaValid[u"country"_qs]    = u"USA"_qs;
    usaValid[u"postalCode"_qs] = u"12345"_qs;
    EXPECT_TRUE(schemaResult->validate(usaValid).isValid());

    QJsonObject usaInvalid{};
    usaInvalid[u"country"_qs]    = u"USA"_qs;
    usaInvalid[u"postalCode"_qs] = u"ABC"_qs;
    EXPECT_FALSE(schemaResult->validate(usaInvalid).isValid());

    QJsonObject otherValid{};
    otherValid[u"country"_qs]    = u"Canada"_qs;
    otherValid[u"postalCode"_qs] = u"K1A 0B1"_qs;
    EXPECT_TRUE(schemaResult->validate(otherValid).isValid());
}

// ============================================================================
// Object Keyword Tests
// ============================================================================

TEST_F(JSONSchemaKeywordTest, PropertiesAndRequired)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "age": {"type": "integer", "minimum": 0}
        },
        "required": ["name"]
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"name"_qs] = u"Alice"_qs;
    valid[u"age"_qs]  = 30;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject validNoAge{};
    validNoAge[u"name"_qs] = u"Bob"_qs;
    EXPECT_TRUE(schemaResult->validate(validNoAge).isValid());

    QJsonObject invalid{};
    invalid[u"age"_qs] = 25;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, AdditionalPropertiesFalse)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "object",
        "properties": {
            "name": {"type": "string"}
        },
        "additionalProperties": false
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"name"_qs] = u"Alice"_qs;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"name"_qs] = u"Alice"_qs;
    invalid[u"age"_qs]  = 30;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, PatternProperties)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "object",
        "patternProperties": {
            "^S_": {"type": "string"},
            "^I_": {"type": "integer"}
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"S_name"_qs] = u"Alice"_qs;
    valid[u"I_age"_qs]  = 30;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"S_name"_qs] = 42;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, MinMaxProperties)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "object",
        "minProperties": 2,
        "maxProperties": 4
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject tooFew{};
    tooFew[u"a"_qs] = 1;
    EXPECT_FALSE(schemaResult->validate(tooFew).isValid());

    QJsonObject valid{};
    valid[u"a"_qs] = 1;
    valid[u"b"_qs] = 2;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject tooMany{};
    tooMany[u"a"_qs] = 1;
    tooMany[u"b"_qs] = 2;
    tooMany[u"c"_qs] = 3;
    tooMany[u"d"_qs] = 4;
    tooMany[u"e"_qs] = 5;
    EXPECT_FALSE(schemaResult->validate(tooMany).isValid());
}

TEST_F(JSONSchemaKeywordTest, PropertyNames)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "object",
        "propertyNames": {
            "pattern": "^[A-Z]"
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"Name"_qs] = u"Alice"_qs;
    valid[u"Age"_qs]  = 30;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"name"_qs] = u"Alice"_qs;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

// ============================================================================
// Array Keyword Tests
// ============================================================================

TEST_F(JSONSchemaKeywordTest, PrefixItems)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "array",
        "prefixItems": [
            {"type": "string"},
            {"type": "number"}
        ]
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonArray valid{};
    valid.append(u"hello"_qs);
    valid.append(42);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonArray invalid{};
    invalid.append(42);
    invalid.append(u"hello"_qs);
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, ItemsSchema)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "array",
        "items": {"type": "integer", "minimum": 0}
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonArray valid{};
    valid.append(1);
    valid.append(2);
    valid.append(3);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonArray invalid{};
    invalid.append(1);
    invalid.append(-1);
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, UniqueItems)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "array",
        "uniqueItems": true
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonArray valid{};
    valid.append(1);
    valid.append(2);
    valid.append(3);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonArray invalid{};
    invalid.append(1);
    invalid.append(2);
    invalid.append(1);
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, Contains)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "array",
        "contains": {"type": "string"}
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonArray valid{};
    valid.append(1);
    valid.append(u"hello"_qs);
    valid.append(3);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonArray invalid{};
    invalid.append(1);
    invalid.append(2);
    invalid.append(3);
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

// ============================================================================
// String Keyword Tests
// ============================================================================

TEST_F(JSONSchemaKeywordTest, StringPattern)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "string",
        "pattern": "^[A-Z][a-z]+$"
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("Hello")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("World")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("HELLO")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("Hello123")).isValid());
}

TEST_F(JSONSchemaKeywordTest, StringMinMaxLength)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "string",
        "minLength": 3,
        "maxLength": 10
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("abc")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("hello")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("0123456789")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("ab")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("01234567890")).isValid());
}

// ============================================================================
// Numeric Keyword Tests
// ============================================================================

TEST_F(JSONSchemaKeywordTest, NumericMinMax)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "number",
        "minimum": 0,
        "maximum": 100
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(50)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(100)).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue(-1)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(101)).isValid());
}

TEST_F(JSONSchemaKeywordTest, ExclusiveMinMax)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "number",
        "exclusiveMinimum": 0,
        "exclusiveMaximum": 100
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(0.1)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(50)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(99.9)).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(100)).isValid());
}

TEST_F(JSONSchemaKeywordTest, MultipleOf)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "number",
        "multipleOf": 5
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(0)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(5)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(10)).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(-15)).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue(3)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(7)).isValid());
}

// ============================================================================
// Value Constraint Tests
// ============================================================================

TEST_F(JSONSchemaKeywordTest, EnumConstraint)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "enum": ["red", "green", "blue"]
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("red")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("green")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("blue")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("yellow")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue(42)).isValid());
}

TEST_F(JSONSchemaKeywordTest, ConstConstraint)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "const": 42
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue(42)).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue(43)).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("42")).isValid());
}

// ============================================================================
// Dependency Tests
// ============================================================================

TEST_F(JSONSchemaKeywordTest, DependentRequired)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "creditCard": {"type": "string"}
        },
        "dependentRequired": {
            "creditCard": ["billingAddress"]
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject validNoCreditCard{};
    validNoCreditCard[u"name"_qs] = u"Alice"_qs;
    EXPECT_TRUE(schemaResult->validate(validNoCreditCard).isValid());

    QJsonObject validWithBilling{};
    validWithBilling[u"name"_qs]           = u"Alice"_qs;
    validWithBilling[u"creditCard"_qs]     = u"1234"_qs;
    validWithBilling[u"billingAddress"_qs] = u"123 Main St"_qs;
    EXPECT_TRUE(schemaResult->validate(validWithBilling).isValid());

    QJsonObject invalidMissingBilling{};
    invalidMissingBilling[u"name"_qs]       = u"Alice"_qs;
    invalidMissingBilling[u"creditCard"_qs] = u"1234"_qs;
    EXPECT_FALSE(schemaResult->validate(invalidMissingBilling).isValid());
}

TEST_F(JSONSchemaKeywordTest, DependentSchemas)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "object",
        "properties": {
            "name": {"type": "string"},
            "creditCard": {"type": "string"}
        },
        "dependentSchemas": {
            "creditCard": {
                "properties": {
                    "billingAddress": {"type": "string"}
                },
                "required": ["billingAddress"]
            }
        }
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject validNoCreditCard{};
    validNoCreditCard[u"name"_qs] = u"Alice"_qs;
    EXPECT_TRUE(schemaResult->validate(validNoCreditCard).isValid());

    QJsonObject validWithBilling{};
    validWithBilling[u"name"_qs]           = u"Alice"_qs;
    validWithBilling[u"creditCard"_qs]     = u"1234"_qs;
    validWithBilling[u"billingAddress"_qs] = u"123 Main St"_qs;
    EXPECT_TRUE(schemaResult->validate(validWithBilling).isValid());

    QJsonObject invalidMissingBilling{};
    invalidMissingBilling[u"name"_qs]       = u"Alice"_qs;
    invalidMissingBilling[u"creditCard"_qs] = u"1234"_qs;
    EXPECT_FALSE(schemaResult->validate(invalidMissingBilling).isValid());
}
