// JSONPathLogicalOrGTest.cpp
// ──────────────────────────
// Sharp tests for logical OR support in JSONPath filters (Qt 6).

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonArray>
#include <ranges>                        // C++20 helpers
#include <json-query/json-path/JSONPath.hpp>

using json_query::JSONPath;

using namespace Qt::StringLiterals;

// ----------------------------------------------------------
//  Shared mini‑dataset (four Bond movies, ids 1‑4)
// ----------------------------------------------------------
static const QJsonDocument bondDoc = []{
    static const char jsonSrc[] = R"JSON([
        {"id": 1, "title": "Casino Royale",      "director": "Martin Campbell",
         "starring": ["Daniel Craig", "Eva Green"],    "release": 2006 },
        {"id": 2, "title": "Quantum of Solace",  "director": "Marc Forster",
         "starring": ["Daniel Craig", "Olga Kurylenko"],"release": 2008 },
        {"id": 3, "title": "Skyfall",            "director": "Sam Mendes",
         "starring": ["Daniel Craig", "Naomie Harris"], "release": 2012 },
        {"id": 4, "title": "Spectre",            "director": "Sam Mendes",
         "starring": ["Daniel Craig", "Lea Seydoux"],   "release": 2015 }
    ])JSON";
    return QJsonDocument::fromJson(QByteArray(jsonSrc));
}();

// ----------------------------------------------------------
// 1. Simple numeric OR
// ----------------------------------------------------------
TEST(JSONPathLogicalOr, IdOneOrThree)
{
    auto path = JSONPath::create(u"$[?(@.id == 1 || @.id == 3)]");
    ASSERT_TRUE(path);
    auto resResult{path->evaluateAll(bondDoc)};
    ASSERT_TRUE(resResult.has_value()) << "Failed to evaluate path";
    QJsonArray res = *resResult;
    ASSERT_EQ(res.size(), 2);

    EXPECT_TRUE(std::ranges::any_of(res, [](const QJsonValue& v){ return v[u"id"_s]==1; }));
    EXPECT_TRUE(std::ranges::any_of(res, [](const QJsonValue& v){ return v[u"id"_s]==3; }));
}

// ----------------------------------------------------------
// 2. Mixed predicate kinds (string equality  OR  “in” operator)
// ----------------------------------------------------------
TEST(JSONPathLogicalOr, DirectorIsMendesOrStarringEva)
{
    auto path = JSONPath::create(
        u"$[?(@['director']=='Sam Mendes' || 'Eva Green' in @['starring'])]");
    ASSERT_TRUE(path);
    auto resResult{path->evaluateAll(bondDoc)};
    ASSERT_TRUE(resResult.has_value()) << "Failed to evaluate path";
    QJsonArray res = *resResult;

    QVector<int> ids;
    for (const auto& v : res) ids << v[u"id"_s].toInt();

    EXPECT_EQ(ids.size(), 3);
    EXPECT_TRUE(ids.contains(1));
    EXPECT_TRUE(ids.contains(3));
    EXPECT_TRUE(ids.contains(4));
}

// ----------------------------------------------------------
// 3. Precedence:  AND binds tighter than OR  (no parens)
// ----------------------------------------------------------
TEST(JSONPathLogicalOr, AndHasHigherPrecedenceThanOr)
{
    auto path = JSONPath::create(
        u"$[?(@.id == 1 || @.director == 'Sam Mendes' && @.id == 3)]");
    ASSERT_TRUE(path);
    auto resResult{path->evaluateAll(bondDoc)};
    ASSERT_TRUE(resResult.has_value()) << "Failed to evaluate path";
    QJsonArray res = *resResult;

    QVector<int> ids;
    for (const auto& v : res) ids << v[u"id"_s].toInt();

    EXPECT_EQ(ids.size(), 2);
    EXPECT_TRUE(ids.contains(1));
    EXPECT_TRUE(ids.contains(3));
    EXPECT_FALSE(ids.contains(4));
}

// ----------------------------------------------------------
// 4. Parenthesised OR inside AND – parentheses override precedence
// ----------------------------------------------------------
TEST(JSONPathLogicalOr, ParenChangePrecedence)
{
    auto path = JSONPath::create(
        u"$[?(@['director']=='Sam Mendes' && (@.id == 3 || @.id == 4))]");
    ASSERT_TRUE(path);
    auto resResult{path->evaluateAll(bondDoc)};
    ASSERT_TRUE(resResult.has_value()) << "Failed to evaluate path";
    QJsonArray res = *resResult;

    ASSERT_EQ(res.size(), 2);
    EXPECT_TRUE(std::ranges::all_of(res,
        [](const QJsonValue& v){ return v[u"director"_s]=="Sam Mendes"; }));

    EXPECT_TRUE(std::ranges::any_of(res, [](const QJsonValue& v){ return v[u"id"_s]==3; }));
    EXPECT_TRUE(std::ranges::any_of(res, [](const QJsonValue& v){ return v[u"id"_s]==4; }));
}
