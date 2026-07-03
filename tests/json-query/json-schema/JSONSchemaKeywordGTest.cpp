// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "json-query/json-schema/JSONSchema.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

using namespace json_query::json_schema;
using json_query::literals::operator""_qt_s;

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
    usaValid[u"country"_qt_s]    = u"USA"_qt_s;
    usaValid[u"postalCode"_qt_s] = u"12345"_qt_s;
    EXPECT_TRUE(schemaResult->validate(usaValid).isValid());

    QJsonObject usaInvalid{};
    usaInvalid[u"country"_qt_s]    = u"USA"_qt_s;
    usaInvalid[u"postalCode"_qt_s] = u"ABC"_qt_s;
    EXPECT_FALSE(schemaResult->validate(usaInvalid).isValid());

    QJsonObject otherValid{};
    otherValid[u"country"_qt_s]    = u"Canada"_qt_s;
    otherValid[u"postalCode"_qt_s] = u"K1A 0B1"_qt_s;
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
    valid[u"name"_qt_s] = u"Alice"_qt_s;
    valid[u"age"_qt_s]  = 30;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject validNoAge{};
    validNoAge[u"name"_qt_s] = u"Bob"_qt_s;
    EXPECT_TRUE(schemaResult->validate(validNoAge).isValid());

    QJsonObject invalid{};
    invalid[u"age"_qt_s] = 25;
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
    valid[u"name"_qt_s] = u"Alice"_qt_s;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"name"_qt_s] = u"Alice"_qt_s;
    invalid[u"age"_qt_s]  = 30;
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
    valid[u"S_name"_qt_s] = u"Alice"_qt_s;
    valid[u"I_age"_qt_s]  = 30;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"S_name"_qt_s] = 42;
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
    tooFew[u"a"_qt_s] = 1;
    EXPECT_FALSE(schemaResult->validate(tooFew).isValid());

    QJsonObject valid{};
    valid[u"a"_qt_s] = 1;
    valid[u"b"_qt_s] = 2;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject tooMany{};
    tooMany[u"a"_qt_s] = 1;
    tooMany[u"b"_qt_s] = 2;
    tooMany[u"c"_qt_s] = 3;
    tooMany[u"d"_qt_s] = 4;
    tooMany[u"e"_qt_s] = 5;
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
    valid[u"Name"_qt_s] = u"Alice"_qt_s;
    valid[u"Age"_qt_s]  = 30;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"name"_qt_s] = u"Alice"_qt_s;
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
    valid.append(u"hello"_qt_s);
    valid.append(42);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonArray invalid{};
    invalid.append(42);
    invalid.append(u"hello"_qt_s);
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
    valid.append(u"hello"_qt_s);
    valid.append(3);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonArray invalid{};
    invalid.append(1);
    invalid.append(2);
    invalid.append(3);
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, MinContains)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "array",
        "contains": {"type": "string"},
        "minContains": 2
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    // 2 strings — meets minimum
    QJsonArray valid{};
    valid.append(1);
    valid.append(u"hello"_qt_s);
    valid.append(u"world"_qt_s);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    // 1 string — below minimum
    QJsonArray tooFew{};
    tooFew.append(1);
    tooFew.append(u"hello"_qt_s);
    tooFew.append(3);
    EXPECT_FALSE(schemaResult->validate(tooFew).isValid());

    // 0 strings — below minimum
    QJsonArray none{};
    none.append(1);
    none.append(2);
    EXPECT_FALSE(schemaResult->validate(none).isValid());
}

TEST_F(JSONSchemaKeywordTest, MaxContains)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "array",
        "contains": {"type": "string"},
        "maxContains": 2
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    // 1 string — within limit
    QJsonArray valid{};
    valid.append(1);
    valid.append(u"hello"_qt_s);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    // 3 strings — exceeds max
    QJsonArray tooMany{};
    tooMany.append(u"a"_qt_s);
    tooMany.append(u"b"_qt_s);
    tooMany.append(u"c"_qt_s);
    EXPECT_FALSE(schemaResult->validate(tooMany).isValid());
}

TEST_F(JSONSchemaKeywordTest, MinContainsZero)
{
    // minContains: 0 means contains is not required to match anything
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "array",
        "contains": {"type": "string"},
        "minContains": 0
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    // No strings at all — valid because minContains is 0
    QJsonArray noStrings{};
    noStrings.append(1);
    noStrings.append(2);
    EXPECT_TRUE(schemaResult->validate(noStrings).isValid());

    // Empty array — valid
    EXPECT_TRUE(schemaResult->validate(QJsonArray{}).isValid());
}

TEST_F(JSONSchemaKeywordTest, MinAndMaxContains)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "type": "array",
        "contains": {"type": "string"},
        "minContains": 1,
        "maxContains": 3
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    // 2 strings — within range
    QJsonArray valid{};
    valid.append(u"a"_qt_s);
    valid.append(1);
    valid.append(u"b"_qt_s);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    // 0 strings — below min
    QJsonArray tooFew{};
    tooFew.append(1);
    tooFew.append(2);
    EXPECT_FALSE(schemaResult->validate(tooFew).isValid());

    // 4 strings — above max
    QJsonArray tooMany{};
    tooMany.append(u"a"_qt_s);
    tooMany.append(u"b"_qt_s);
    tooMany.append(u"c"_qt_s);
    tooMany.append(u"d"_qt_s);
    EXPECT_FALSE(schemaResult->validate(tooMany).isValid());
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
    validNoCreditCard[u"name"_qt_s] = u"Alice"_qt_s;
    EXPECT_TRUE(schemaResult->validate(validNoCreditCard).isValid());

    QJsonObject validWithBilling{};
    validWithBilling[u"name"_qt_s]           = u"Alice"_qt_s;
    validWithBilling[u"creditCard"_qt_s]     = u"1234"_qt_s;
    validWithBilling[u"billingAddress"_qt_s] = u"123 Main St"_qt_s;
    EXPECT_TRUE(schemaResult->validate(validWithBilling).isValid());

    QJsonObject invalidMissingBilling{};
    invalidMissingBilling[u"name"_qt_s]       = u"Alice"_qt_s;
    invalidMissingBilling[u"creditCard"_qt_s] = u"1234"_qt_s;
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
    validNoCreditCard[u"name"_qt_s] = u"Alice"_qt_s;
    EXPECT_TRUE(schemaResult->validate(validNoCreditCard).isValid());

    QJsonObject validWithBilling{};
    validWithBilling[u"name"_qt_s]           = u"Alice"_qt_s;
    validWithBilling[u"creditCard"_qt_s]     = u"1234"_qt_s;
    validWithBilling[u"billingAddress"_qt_s] = u"123 Main St"_qt_s;
    EXPECT_TRUE(schemaResult->validate(validWithBilling).isValid());

    QJsonObject invalidMissingBilling{};
    invalidMissingBilling[u"name"_qt_s]       = u"Alice"_qt_s;
    invalidMissingBilling[u"creditCard"_qt_s] = u"1234"_qt_s;
    EXPECT_FALSE(schemaResult->validate(invalidMissingBilling).isValid());
}

// ============================================================================
// Unevaluated Properties Tests
// ============================================================================

TEST_F(JSONSchemaKeywordTest, UnevaluatedPropertiesFalse)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "properties": {"foo": {"type": "string"}},
        "unevaluatedProperties": false
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"foo"_qt_s] = u"bar"_qt_s;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"foo"_qt_s]   = u"bar"_qt_s;
    invalid[u"extra"_qt_s] = 1;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, UnevaluatedPropertiesWithAllOf)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "properties": {"foo": {"type": "string"}},
        "allOf": [{"properties": {"bar": {"type": "string"}}}],
        "unevaluatedProperties": false
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"foo"_qt_s] = u"a"_qt_s;
    valid[u"bar"_qt_s] = u"b"_qt_s;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"foo"_qt_s] = u"a"_qt_s;
    invalid[u"bar"_qt_s] = u"b"_qt_s;
    invalid[u"baz"_qt_s] = u"c"_qt_s;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, UnevaluatedPropertiesWithAnyOf)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "properties": {"foo": {"type": "string"}},
        "anyOf": [
            {"properties": {"bar": {"const": "bar"}}, "required": ["bar"]},
            {"properties": {"baz": {"const": "baz"}}, "required": ["baz"]}
        ],
        "unevaluatedProperties": false
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"foo"_qt_s] = u"foo"_qt_s;
    valid[u"bar"_qt_s] = u"bar"_qt_s;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"foo"_qt_s]   = u"foo"_qt_s;
    invalid[u"bar"_qt_s]   = u"bar"_qt_s;
    invalid[u"extra"_qt_s] = 1;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, UnevaluatedPropertiesWithIfThenElse)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "properties": {"foo": {"type": "string"}},
        "if": {"properties": {"foo": {"const": "yes"}}},
        "then": {"properties": {"bar": {"type": "string"}}},
        "else": {"properties": {"baz": {"type": "string"}}},
        "unevaluatedProperties": false
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    // if matches -> then branch evaluates "bar"
    QJsonObject validThen{};
    validThen[u"foo"_qt_s] = u"yes"_qt_s;
    validThen[u"bar"_qt_s] = u"b"_qt_s;
    EXPECT_TRUE(schemaResult->validate(validThen).isValid());

    // if doesn't match -> else branch evaluates "baz"
    QJsonObject validElse{};
    validElse[u"foo"_qt_s] = u"no"_qt_s;
    validElse[u"baz"_qt_s] = u"c"_qt_s;
    EXPECT_TRUE(schemaResult->validate(validElse).isValid());

    // extra property not evaluated by any branch
    QJsonObject invalid{};
    invalid[u"foo"_qt_s]   = u"yes"_qt_s;
    invalid[u"bar"_qt_s]   = u"b"_qt_s;
    invalid[u"extra"_qt_s] = 1;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, UnevaluatedPropertiesSchema)
{
    // unevaluatedProperties as a schema (not false) validates remaining properties
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "properties": {"foo": {"type": "string"}},
        "unevaluatedProperties": {"type": "number"}
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonObject valid{};
    valid[u"foo"_qt_s]   = u"bar"_qt_s;
    valid[u"extra"_qt_s] = 42;
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonObject invalid{};
    invalid[u"foo"_qt_s]   = u"bar"_qt_s;
    invalid[u"extra"_qt_s] = u"not a number"_qt_s;
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

// ============================================================================
// Unevaluated Items Tests
// ============================================================================

TEST_F(JSONSchemaKeywordTest, UnevaluatedItemsFalseWithPrefixItems)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "prefixItems": [{"type": "string"}, {"type": "number"}],
        "unevaluatedItems": false
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonArray valid{};
    valid.append(u"hello"_qt_s);
    valid.append(42);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonArray invalid{};
    invalid.append(u"hello"_qt_s);
    invalid.append(42);
    invalid.append(true);
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, UnevaluatedItemsWithItems)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "prefixItems": [{"type": "string"}],
        "items": {"type": "number"},
        "unevaluatedItems": false
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    // All items evaluated by prefixItems + items
    QJsonArray valid{};
    valid.append(u"hello"_qt_s);
    valid.append(1);
    valid.append(2);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());
}

TEST_F(JSONSchemaKeywordTest, UnevaluatedItemsWithAllOf)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "prefixItems": [{"type": "string"}],
        "allOf": [{"prefixItems": [true, {"type": "number"}]}],
        "unevaluatedItems": false
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonArray valid{};
    valid.append(u"hello"_qt_s);
    valid.append(42);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonArray invalid{};
    invalid.append(u"hello"_qt_s);
    invalid.append(42);
    invalid.append(u"extra"_qt_s);
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}

TEST_F(JSONSchemaKeywordTest, UnevaluatedItemsSchema)
{
    // unevaluatedItems as a schema validates remaining items
    auto schemaResult{JSONSchema::create(parseSchema(R"({
        "prefixItems": [{"type": "string"}],
        "unevaluatedItems": {"type": "number"}
    })"))};
    ASSERT_TRUE(schemaResult.has_value());

    QJsonArray valid{};
    valid.append(u"hello"_qt_s);
    valid.append(42);
    EXPECT_TRUE(schemaResult->validate(valid).isValid());

    QJsonArray invalid{};
    invalid.append(u"hello"_qt_s);
    invalid.append(u"not a number"_qt_s);
    EXPECT_FALSE(schemaResult->validate(invalid).isValid());
}
