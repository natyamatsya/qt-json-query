// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "json-query/json-schema/SchemaRegistry.hpp"

using namespace json_query::json_schema;
using namespace Qt::StringLiterals;

class SchemaRegistryTest : public ::testing::Test
{
  protected:
    static QJsonValue parseSchema(const char* json) { return QJsonDocument::fromJson(json).object(); }

    SchemaRegistry registry{};
};

// ============================================================================
// Basic Registry Operations
// ============================================================================

TEST_F(SchemaRegistryTest, DefaultConstructedRegistryIsEmpty)
{
    EXPECT_EQ(registry.compiledCount(), 0u);
    EXPECT_EQ(registry.documentCount(), 0u);
}

TEST_F(SchemaRegistryTest, AddSchemaByUri)
{
    const auto schema{parseSchema(R"({"type": "string"})")};
    const auto result{registry.add(u"https://example.com/string-schema"_s, schema)};

    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(registry.contains(u"https://example.com/string-schema"_s));
    EXPECT_EQ(registry.compiledCount(), 1u);
}

TEST_F(SchemaRegistryTest, AddSchemaAlsoCachesDocument)
{
    const auto schema{parseSchema(R"({"type": "integer"})")};
    registry.add(u"https://example.com/int-schema"_s, schema);

    EXPECT_TRUE(registry.containsDocument(u"https://example.com/int-schema"_s));
    EXPECT_EQ(registry.documentCount(), 1u);
}

TEST_F(SchemaRegistryTest, GetReturnsNulloptForUnknownUri)
{
    EXPECT_FALSE(registry.get(u"https://example.com/nonexistent"_s).has_value());
}

TEST_F(SchemaRegistryTest, GetReturnsCachedSchema)
{
    const auto schema{parseSchema(R"({"type": "string"})")};
    registry.add(u"https://example.com/s"_s, schema);

    const auto cached{registry.get(u"https://example.com/s"_s)};
    ASSERT_TRUE(cached.has_value());
    EXPECT_TRUE(cached->validate(QJsonValue("hello")).isValid());
    EXPECT_FALSE(cached->validate(QJsonValue(42)).isValid());
}

// ============================================================================
// Tier 1: Compiled Schema Caching
// ============================================================================

TEST_F(SchemaRegistryTest, CreateCachesBySchemaId)
{
    const auto schema{parseSchema(R"({
        "$id": "https://example.com/cached",
        "type": "number"
    })")};

    const auto result{registry.create(schema)};
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(registry.contains(u"https://example.com/cached"_s));
}

TEST_F(SchemaRegistryTest, CreateReturnsCachedOnSecondCall)
{
    const auto schema{parseSchema(R"({
        "$id": "https://example.com/dedup",
        "type": "boolean"
    })")};

    const auto first{registry.create(schema)};
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(registry.compiledCount(), 1u);

    // Second call should hit Tier 1 cache
    const auto second{registry.create(schema)};
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(registry.compiledCount(), 1u); // no new entry

    // Both should validate identically
    EXPECT_TRUE(second->validate(QJsonValue(true)).isValid());
    EXPECT_FALSE(second->validate(QJsonValue("not-bool")).isValid());
}

TEST_F(SchemaRegistryTest, CreateWithoutIdDoesNotCache)
{
    const auto schema{parseSchema(R"({"type": "string"})")};

    registry.create(schema);
    EXPECT_EQ(registry.compiledCount(), 0u);
}

TEST_F(SchemaRegistryTest, AddCachesUnderBothUriAndSchemaId)
{
    const auto schema{parseSchema(R"({
        "$id": "https://example.com/actual-id",
        "type": "string"
    })")};

    registry.add(u"https://example.com/alias"_s, schema);

    EXPECT_TRUE(registry.contains(u"https://example.com/alias"_s));
    EXPECT_TRUE(registry.contains(u"https://example.com/actual-id"_s));
}

// ============================================================================
// Tier 2: Document Fetch Caching
// ============================================================================

TEST_F(SchemaRegistryTest, CachingFetcherAvoidsRedundantFetches)
{
    int fetchCount{0};
    registry.setFetcher([&fetchCount](const QString& uri) -> std::optional<QJsonValue>
    {
        if (uri == u"https://example.com/remote-type")
        {
            ++fetchCount;
            return QJsonDocument::fromJson(R"({"type": "integer"})").object();
        }
        return std::nullopt;
    });

    // Schema that references a remote type
    const auto schema1{parseSchema(R"({
        "$ref": "https://example.com/remote-type"
    })")};

    const auto result1{registry.create(schema1)};
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(fetchCount, 1);
    EXPECT_TRUE(registry.containsDocument(u"https://example.com/remote-type"_s));

    // Second schema also referencing the same remote
    const auto schema2{parseSchema(R"({
        "properties": {
            "count": { "$ref": "https://example.com/remote-type" }
        }
    })")};

    const auto result2{registry.create(schema2)};
    ASSERT_TRUE(result2.has_value());
    // Should NOT have fetched again — served from document cache
    EXPECT_EQ(fetchCount, 1);
}

TEST_F(SchemaRegistryTest, AddedDocumentServesAsFetchCache)
{
    int fetchCount{0};
    registry.setFetcher([&fetchCount](const QString&) -> std::optional<QJsonValue>
    {
        ++fetchCount;
        return std::nullopt;
    });

    // Pre-register a schema
    const auto remoteSchema{parseSchema(R"({
        "$id": "https://example.com/defs",
        "type": "object"
    })")};
    registry.add(u"https://example.com/defs"_s, remoteSchema);

    // Now compile a schema that references it — should not call the fetcher
    const auto schema{parseSchema(R"({
        "$ref": "https://example.com/defs"
    })")};
    registry.create(schema);

    EXPECT_EQ(fetchCount, 0);
}

// ============================================================================
// Remove and Clear
// ============================================================================

TEST_F(SchemaRegistryTest, RemoveDeletesBothCaches)
{
    const auto schema{parseSchema(R"({"type": "string"})")};
    registry.add(u"https://example.com/to-remove"_s, schema);

    EXPECT_TRUE(registry.contains(u"https://example.com/to-remove"_s));
    EXPECT_TRUE(registry.containsDocument(u"https://example.com/to-remove"_s));

    EXPECT_TRUE(registry.remove(u"https://example.com/to-remove"_s));
    EXPECT_FALSE(registry.contains(u"https://example.com/to-remove"_s));
    EXPECT_FALSE(registry.containsDocument(u"https://example.com/to-remove"_s));
}

TEST_F(SchemaRegistryTest, RemoveReturnsFalseForUnknown)
{
    EXPECT_FALSE(registry.remove(u"https://example.com/nonexistent"_s));
}

TEST_F(SchemaRegistryTest, ClearRemovesEverything)
{
    registry.add(u"https://example.com/a"_s, parseSchema(R"({"type": "string"})"));
    registry.add(u"https://example.com/b"_s, parseSchema(R"({"type": "number"})"));

    EXPECT_EQ(registry.compiledCount(), 2u);
    EXPECT_EQ(registry.documentCount(), 2u);

    registry.clear();

    EXPECT_EQ(registry.compiledCount(), 0u);
    EXPECT_EQ(registry.documentCount(), 0u);
}

// ============================================================================
// Validation Through Registry
// ============================================================================

TEST_F(SchemaRegistryTest, CachedSchemaValidatesCorrectly)
{
    const auto schema{parseSchema(R"({
        "$id": "https://example.com/person",
        "type": "object",
        "properties": {
            "name": { "type": "string" },
            "age": { "type": "integer", "minimum": 0 }
        },
        "required": ["name"]
    })")};

    registry.add(u"https://example.com/person"_s, schema);

    const auto cached{registry.get(u"https://example.com/person"_s)};
    ASSERT_TRUE(cached.has_value());

    // Valid instance
    const auto validInstance{QJsonDocument::fromJson(R"({"name": "Alice", "age": 30})").object()};
    EXPECT_TRUE(cached->validate(QJsonValue(validInstance)).isValid());

    // Missing required
    const auto missingName{QJsonDocument::fromJson(R"({"age": 30})").object()};
    EXPECT_FALSE(cached->validate(QJsonValue(missingName)).isValid());

    // Wrong type
    EXPECT_FALSE(cached->validate(QJsonValue("not an object")).isValid());
}

TEST_F(SchemaRegistryTest, BooleanSchemaThroughRegistry)
{
    const auto trueResult{registry.create(QJsonValue(true))};
    ASSERT_TRUE(trueResult.has_value());
    EXPECT_TRUE(trueResult->validate(QJsonValue(42)).isValid());

    const auto falseResult{registry.create(QJsonValue(false))};
    ASSERT_TRUE(falseResult.has_value());
    EXPECT_FALSE(falseResult->validate(QJsonValue(42)).isValid());
}
