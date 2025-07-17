// DeepScanParityGTest.cpp
// Ported from Jayway JSONPath test: com/jayway/jsonpath/DeepScanTest.java
// Source repository: tests/references/jayway-json-path/json-path/src/test/java/com/jayway/jsonpath/DeepScanTest.java
// Only the two simplest test methods are included here. Further cases can be
// added incrementally.

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <QJsonDocument>
#include "json-query/JSONPath.hpp"
#include "JaywayParityGTestHelpers.hpp"
#include <QJsonValue>


TEST(JaywayDeepScanParity, NonArraySubscriptionIgnored)
{
    auto path = JSONPath::create(u"$..[2][3]");
    ASSERT_TRUE(path);

    {
        QJsonArray result = jp::evalArray(*path, jp::parseJson(R"({"x": [0,1,[0,1,2,3,null],null]})"));
        EXPECT_THAT(result, ::testing::ElementsAre(IsJsonInt(3)));
    }
    {
        QJsonArray result = jp::evalArray(*path, jp::parseJson(R"({"x": [0,1,[0,1,2,3,null],null], "y": [0,1,2]})"));
        EXPECT_THAT(result, ::testing::ElementsAre(IsJsonInt(3)));
    }
    {
        QJsonArray result = jp::evalArray(*path, jp::parseJson(R"({"x": [0,1,[0,1,2],null], "y": [0,1,2]})"));
        EXPECT_THAT(result, ::testing::IsEmpty());
    }
}

TEST(JaywayDeepScanParity, NullSubscriptionIgnored)
{
    auto path = JSONPath::create(u"$..[2][3]");
    ASSERT_TRUE(path);

    {
        QJsonArray result = jp::evalArray(*path, jp::parseJson(R"({"x": [null,null,[0,1,2,3,null],null]})"));
        EXPECT_THAT(result, ::testing::ElementsAre(IsJsonInt(3)));
    }
    {
        QJsonArray result = jp::evalArray(*path, jp::parseJson(R"({"x": [null,null,[0,1,2,3,null],null], "y": [0,1,null]})"));
        EXPECT_THAT(result, ::testing::ElementsAre(IsJsonInt(3)));
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
    QJsonArray r1 = jp::evalArray(*path1, jp::parseJson(R"({"x": [0,1,[0,1,2,3,10],null]})"));
    EXPECT_THAT(r1, ::testing::ElementsAre(IsJsonInt(10)));

    auto path2 = JSONPath::create(u"$..[2][3]");
    ASSERT_TRUE(path2);
    QJsonArray r2 = jp::evalArray(*path2, jp::parseJson(R"({"x": [null,null,[0,1,2,3]], "y": [null,null,[0,1]]})"));
    EXPECT_THAT(r2, ::testing::ElementsAre(IsJsonInt(3)));
}

PARITY_TEST(DefiniteUpstreamIllegalArrayAccessThrows, "Awaiting PathNotFoundException support.");
TEST(JaywayDeepScanParity, DISABLED_IllegalPropertyAccessIgnored)
{
    // $..foo should collect both objects and scalars
    QJsonArray r1 = jp::evalArray(*JSONPath::create(u"$..foo"),
        jp::parseJson(R"({"x": {"foo": {"bar": 4}}, "y": {"foo": 1}})"));
    ASSERT_EQ(r1.size(), 2);

    // $..foo.bar should only return the nested bar property
    QJsonArray r2 = jp::evalArray(*JSONPath::create(u"$..foo.bar"),
        jp::parseJson(R"({"x": {"foo": {"bar": 4}}, "y": {"foo": 1}})"));
    EXPECT_THAT(r2, ::testing::ElementsAre(IsJsonInt(4)));

    QJsonArray r3 = jp::evalArray(*JSONPath::create(u"$..[*].foo.bar"),
        jp::parseJson(R"({"x": {"foo": {"bar": 4}}, "y": {"foo": 1}})"));
    EXPECT_THAT(r3, ::testing::ElementsAre(IsJsonInt(4)));

    QJsonArray r4 = jp::evalArray(*JSONPath::create(u"$..[*].foo.bar"),
        jp::parseJson(R"({"x": {"foo": {"baz": 4}}, "y": {"foo": 1}})"));
    EXPECT_TRUE(r4.isEmpty());
}

TEST(JaywayDeepScanParity, IllegalPredicateIgnored)
{
    // Predicate selects objects having a bar property, then extracts bar
    auto path = JSONPath::create(u"$..foo[?(@.bar)].bar");
    ASSERT_TRUE(path);
    QJsonArray result = jp::evalArray(*path, jp::parseJson(R"({"x": {"foo": {"bar": 4}}, "y": {"foo": 1}})"));
    EXPECT_THAT(result, ::testing::ElementsAre(IsJsonInt(4)));
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
    QJsonArray result = jp::evalArray(*path, jp::parseJson(jsonSrc));
    EXPECT_THAT(result, ::testing::ElementsAre(JsonObjContains(std::initializer_list<std::pair<QString, QJsonValue>>{{QStringLiteral("a"), QJsonValue("a-val")}, {QStringLiteral("b"), QJsonValue("b-val")}, {QStringLiteral("c"), QJsonValue("c-val")}})));
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
    QJsonArray result = jp::evalArray(*path, jp::parseJson(jsonSrc));
    // Expect: "aa", {"a":"aa"}, "aa"
    EXPECT_THAT(result, ::testing::SizeIs(3));
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
    QJsonArray result = jp::evalArray(*path, jp::parseJson(jsonSrc));
    EXPECT_THAT(result, ::testing::ElementsAre(IsJsonString(QStringLiteral("xx")), IsJsonString(QStringLiteral("xx"))));
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
    QJsonArray result = jp::evalArray(*path, jp::parseJson(jsonSrc));
    ASSERT_EQ(result.size(), 2);
    EXPECT_TRUE(result[0].isObject());
    EXPECT_TRUE(result[1].isObject());
}

PARITY_TEST(ScanWithFunctionFilter, "Requires function filter implementation.");
PARITY_TEST(DeepScanPathDefault, "Pending executeScanPath default implementation.");
PARITY_TEST(DeepScanPathRequireProperties, "Pending executeScanPath with REQUIRE_PROPERTIES option.");

#undef PARITY_TEST
