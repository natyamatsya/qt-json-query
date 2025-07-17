// DeepScanParityGTest.cpp
// Ported from Jayway JSONPath test: com/jayway/jsonpath/DeepScanTest.java
// Source repository: tests/references/jayway-json-path/json-path/src/test/java/com/jayway/jsonpath/DeepScanTest.java
// Only the two simplest test methods are included here. Further cases can be
// added incrementally.

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonArray>
#include "json-query/JSONPath.hpp"
#include <QJsonValue>

// Helper: parse JSON text into a QJsonDocument
static QJsonDocument parseJson(const char* src)
{
    return QJsonDocument::fromJson(QByteArray(src));
}

// Helper: evaluate a path and return the resulting QJsonArray (flattening if the
// implementation returns a single value)
static QJsonArray evalArray(const JSONPath& path, const QJsonDocument& doc)
{
    const QJsonValue v = path.evaluate(doc);
    if (v.isArray())
        return v.toArray();
    return QJsonArray{v};
}

TEST(JaywayDeepScanParity, NonArraySubscriptionIgnored)
{
    auto path = JSONPath::create(u"$..[2][3]");
    ASSERT_TRUE(path);

    {
        QJsonArray result = evalArray(*path, parseJson(R"({"x": [0,1,[0,1,2,3,null],null]})"));
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0].toInt(), 3);
    }
    {
        QJsonArray result = evalArray(*path, parseJson(R"({"x": [0,1,[0,1,2,3,null],null], "y": [0,1,2]})"));
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0].toInt(), 3);
    }
    {
        QJsonArray result = evalArray(*path, parseJson(R"({"x": [0,1,[0,1,2],null], "y": [0,1,2]})"));
        EXPECT_TRUE(result.isEmpty());
    }
}

TEST(JaywayDeepScanParity, NullSubscriptionIgnored)
{
    auto path = JSONPath::create(u"$..[2][3]");
    ASSERT_TRUE(path);

    {
        QJsonArray result = evalArray(*path, parseJson(R"({"x": [null,null,[0,1,2,3,null],null]})"));
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0].toInt(), 3);
    }
    {
        QJsonArray result = evalArray(*path, parseJson(R"({"x": [null,null,[0,1,2,3,null],null], "y": [0,1,null]})"));
        ASSERT_EQ(result.size(), 1);
        EXPECT_EQ(result[0].toInt(), 3);
    }
}

// -----------------------------------------------------------------------------
// Remaining parity tests from DeepScanTest.java – currently disabled pending
// implementation of required features. Remove DISABLED_ prefix and replace
// GTEST_SKIP() with real assertions once supported.

#define PARITY_TEST(name, reason) \
    TEST(JaywayDeepScanParity, DISABLED_##name) { GTEST_SKIP() << reason; }

TEST(JaywayDeepScanParity, ArrayIndexOobIgnored)
{
    auto path1 = JSONPath::create(u"$..[4]");
    ASSERT_TRUE(path1);
    QJsonArray r1 = evalArray(*path1, parseJson(R"({"x": [0,1,[0,1,2,3,10],null]})"));
    ASSERT_EQ(r1.size(), 1);
    EXPECT_EQ(r1[0].toInt(), 10);

    auto path2 = JSONPath::create(u"$..[2][3]");
    ASSERT_TRUE(path2);
    QJsonArray r2 = evalArray(*path2, parseJson(R"({"x": [null,null,[0,1,2,3]], "y": [null,null,[0,1]]})"));
    ASSERT_EQ(r2.size(), 1);
    EXPECT_EQ(r2[0].toInt(), 3);
}

PARITY_TEST(DefiniteUpstreamIllegalArrayAccessThrows, "Awaiting PathNotFoundException support.");
TEST(JaywayDeepScanParity, DISABLED_IllegalPropertyAccessIgnored)
{
    // $..foo should collect both objects and scalars
    QJsonArray r1 = evalArray(*JSONPath::create(u"$..foo"),
        parseJson(R"({"x": {"foo": {"bar": 4}}, "y": {"foo": 1}})"));
    ASSERT_EQ(r1.size(), 2);

    // $..foo.bar should only return the nested bar property
    QJsonArray r2 = evalArray(*JSONPath::create(u"$..foo.bar"),
        parseJson(R"({"x": {"foo": {"bar": 4}}, "y": {"foo": 1}})"));
    ASSERT_EQ(r2.size(), 1);
    EXPECT_EQ(r2[0].toInt(), 4);

    QJsonArray r3 = evalArray(*JSONPath::create(u"$..[*].foo.bar"),
        parseJson(R"({"x": {"foo": {"bar": 4}}, "y": {"foo": 1}})"));
    ASSERT_EQ(r3.size(), 1);
    EXPECT_EQ(r3[0].toInt(), 4);

    QJsonArray r4 = evalArray(*JSONPath::create(u"$..[*].foo.bar"),
        parseJson(R"({"x": {"foo": {"baz": 4}}, "y": {"foo": 1}})"));
    EXPECT_TRUE(r4.isEmpty());
}
TEST(JaywayDeepScanParity, IllegalPredicateIgnored)
{
    // Predicate selects objects having a bar property, then extracts bar
    auto path = JSONPath::create(u"$..foo[?(@.bar)].bar");
    ASSERT_TRUE(path);
    QJsonArray result = evalArray(*path, parseJson(R"({"x": {"foo": {"bar": 4}}, "y": {"foo": 1}})"));
    ASSERT_EQ(result.size(), 1);
    EXPECT_EQ(result[0].toInt(), 4);
}

PARITY_TEST(RequirePropertiesIgnoredOnScanTarget, "Requires Option::REQUIRE_PROPERTIES flag handling.");
PARITY_TEST(RequirePropertiesIgnoredOnScanTargetButNotChildren, "Requires Option::REQUIRE_PROPERTIES nested handling.");
TEST(JaywayDeepScanParity, LeafMultiPropsWork)
{
    const char* jsonSrc = R"([
        {"a": "a-val", "b": "b-val", "c": "c-val"},
        [1,5],
        {"a": "a-val"}
    ])";

    auto path = JSONPath::create(u"$..['a','c']");
    ASSERT_TRUE(path);
    QJsonArray result = evalArray(*path, parseJson(jsonSrc));
    // Expected behaviour: only objects containing *all* requested properties
    ASSERT_EQ(result.size(), 1);
    QJsonValue obj = result[0];
    ASSERT_TRUE(obj.isObject());
    auto o = obj.toObject();
    EXPECT_EQ(o.size(), 3);
    EXPECT_EQ(o["a"].toString(), u"a-val");
    EXPECT_EQ(o["b"].toString(), u"b-val");
    EXPECT_EQ(o["c"].toString(), u"c-val");
}

PARITY_TEST(RequireSinglePropertyOk, "Requires property requirement enforcement.");
PARITY_TEST(RequireSingleProperty, "Requires property requirement enforcement.");
PARITY_TEST(RequireMultiPropertyAllMatch, "Requires multi-property all-match enforcement.");
PARITY_TEST(RequireMultiPropertySomeMatch, "Requires multi-property partial-match enforcement.");
TEST(JaywayDeepScanParity, ScanForSingleProperty)
{
    const char* jsonSrc = R"([
        {"a": "aa"},
        {"b": "bb"},
        {"b": "bb", "ab": {"a": "aa", "b": "bb"}}
    ])";
    auto path = JSONPath::create(u"$..['a']");
    ASSERT_TRUE(path);
    QJsonArray result = evalArray(*path, parseJson(jsonSrc));
    // Expect: "aa", {"a":"aa"}, "aa"
    ASSERT_EQ(result.size(), 3);
    EXPECT_TRUE(result[0].isString());
    EXPECT_EQ(result[0].toString(), u"aa");
    EXPECT_TRUE(result[1].isObject());
    EXPECT_TRUE(result[2].isString());
    EXPECT_EQ(result[2].toString(), u"aa");
}

TEST(JaywayDeepScanParity, ScanForPropertyPath)
{
    const char* jsonSrc = R"([
        {"a": "aa"},
        {"x": "xx"},
        {"a": {"x": "xx"}},
        {"z": {"a": {"x": "xx"}}}
    ])";
    auto path = JSONPath::create(u"$..['a'].x");
    ASSERT_TRUE(path);
    QJsonArray result = evalArray(*path, parseJson(jsonSrc));
    ASSERT_EQ(result.size(), 2);
    EXPECT_EQ(result[0].toString(), u"xx");
    EXPECT_EQ(result[1].toString(), u"xx");
}

TEST(JaywayDeepScanParity, ScansCanBeFiltered)
{
    const char* jsonSrc = R"([
        {"mammal": true,  "color": {"val": "brown"}},
        {"mammal": true,  "color": {"val": "white"}},
        {"mammal": false}
    ])";
    auto path = JSONPath::create(u"$..[?(@.mammal == true)].color");
    ASSERT_TRUE(path);
    QJsonArray result = evalArray(*path, parseJson(jsonSrc));
    ASSERT_EQ(result.size(), 2);
    EXPECT_TRUE(result[0].isObject());
    EXPECT_TRUE(result[1].isObject());
}

PARITY_TEST(ScanWithFunctionFilter, "Requires function filter implementation.");
PARITY_TEST(DeepScanPathDefault, "Pending executeScanPath default implementation.");
PARITY_TEST(DeepScanPathRequireProperties, "Pending executeScanPath with REQUIRE_PROPERTIES option.");

#undef PARITY_TEST
