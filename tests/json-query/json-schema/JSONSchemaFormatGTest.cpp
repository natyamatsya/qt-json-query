// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>

#include "json-query/json-schema/JSONSchema.hpp"

using namespace json_query::json_schema;

class JSONSchemaFormatTest : public ::testing::Test
{
  protected:
    static QJsonObject parseSchema(const char* json) { return QJsonDocument::fromJson(json).object(); }
};

TEST_F(JSONSchemaFormatTest, DateTimeFormat)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "date-time"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("2024-01-04T23:33:00Z")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("2024-01-04T23:33:00.123Z")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("2024-01-04T23:33:00+01:00")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("2024-01-04T23:33:00-05:00")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("2024-01-04")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("23:33:00")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("not-a-date")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("2024/01/04 23:33:00")).isValid());
}

TEST_F(JSONSchemaFormatTest, DateFormat)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "date"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("2024-01-04")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("1999-12-31")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("2000-01-01")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("2024-01-04T23:33:00Z")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("01-04-2024")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("2024/01/04")).isValid());
}

TEST_F(JSONSchemaFormatTest, TimeFormat)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "time"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("23:33:00Z")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("23:33:00.123Z")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("23:33:00+01:00")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("23:33:00-05:00")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("23:33:00")).isValid());     // no timezone
    EXPECT_FALSE(schemaResult->validate(QJsonValue("23:33:00.123")).isValid()); // no timezone
    EXPECT_FALSE(schemaResult->validate(QJsonValue("2024-01-04T23:33:00Z")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("25:00:00Z")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("23:60:00Z")).isValid());
}

TEST_F(JSONSchemaFormatTest, EmailFormat)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "email"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("user@example.com")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("test.user@example.co.uk")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("user+tag@example.com")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("a@b.c")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("not-an-email")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("@example.com")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("user@")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("user @example.com")).isValid());
}

TEST_F(JSONSchemaFormatTest, HostnameFormat)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "hostname"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("example.com")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("sub.example.com")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("localhost")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("my-server.example.com")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("-example.com")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("example-.com")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("example..com")).isValid());
}

TEST_F(JSONSchemaFormatTest, IPv4Format)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "ipv4"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("192.168.1.1")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("127.0.0.1")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("0.0.0.0")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("255.255.255.255")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("256.1.1.1")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("192.168.1")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("192.168.1.1.1")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("not-an-ip")).isValid());
}

TEST_F(JSONSchemaFormatTest, IPv6Format)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "ipv6"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("2001:0db8:85a3:0000:0000:8a2e:0370:7334")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("::1")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("::")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("fe80::1")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("192.168.1.1")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("gggg::1")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("not-an-ipv6")).isValid());
}

TEST_F(JSONSchemaFormatTest, UriFormat)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "uri"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("https://example.com")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("http://example.com/path")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("ftp://files.example.com")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("file:///path/to/file")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("/relative/path")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("not a uri")).isValid());
}

TEST_F(JSONSchemaFormatTest, UuidFormat)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "uuid"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("550e8400-e29b-41d4-a716-446655440000")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("6ba7b810-9dad-11d1-80b4-00c04fd430c8")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("00000000-0000-0000-0000-000000000000")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("550e8400-e29b-41d4-a716")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("550e8400e29b41d4a716446655440000")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("not-a-uuid")).isValid());
}

TEST_F(JSONSchemaFormatTest, JsonPointerFormat)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "json-pointer"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("/foo")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("/foo/bar")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("/foo/0")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("/a~0b")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("/a~1b")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("foo")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("/foo~")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("/foo~2")).isValid());
}

TEST_F(JSONSchemaFormatTest, RegexFormat)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "regex"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("^[a-z]+$")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("\\d{3}-\\d{4}")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue(".*")).isValid());

    EXPECT_FALSE(schemaResult->validate(QJsonValue("[invalid")).isValid());
    EXPECT_FALSE(schemaResult->validate(QJsonValue("(unclosed")).isValid());
}

TEST_F(JSONSchemaFormatTest, UnknownFormatDoesNotFail)
{
    auto schemaResult{JSONSchema::create(parseSchema(R"({"type": "string", "format": "unknown-format"})"))};
    ASSERT_TRUE(schemaResult.has_value());

    EXPECT_TRUE(schemaResult->validate(QJsonValue("any value")).isValid());
    EXPECT_TRUE(schemaResult->validate(QJsonValue("")).isValid());
}
