// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// JSONPathBaeldungGTest.cpp - Tests adapted from Baeldung JsonPath tutorial
// Article: https://www.baeldung.com/guide-to-jayway-jsonpath
// These tests validate that our C++ JSONPath implementation behaves equivalently
// to the Java examples shown in the article. Some features (e.g. 'in' operator,
// Option.AS_PATH_LIST) are currently not implemented – such tests are skipped.

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <gtest/gtest-spi.h>
#include <algorithm>
#include "framework/JSONMatchersGTest.hpp"
#include "json-query/JSONQuery" // For public API aliases

using namespace Qt::StringLiterals;
using namespace ::testing;

// Using declarations for convenience
using json_query::json_path::JSONPath;

//---------------------------------------------
// 4.1  Access to Documents
//---------------------------------------------
TEST(JSONPathBaeldung, CreatorNameAndLocation)
{
    static const char jsonSrc[] = R"JSON({
        "tool": {
            "jsonpath": {
                "creator": {
                    "name": "Jayway Inc.",
                    "location": ["Malmo", "San Francisco", "Helsingborg"]
                }
            }
        },
        "book": [
            {"title": "Beginning JSON", "price": 49.99},
            {"title": "JSON at Work", "price": 29.99}
        ]
    })JSON";

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonSrc));

    auto namePath{JSONPath::create(u"$['tool']['jsonpath']['creator']['name']")};
    auto locationPath{JSONPath::create(u"$['tool']['jsonpath']['creator']['location'][*]")};

    ASSERT_TRUE(namePath);
    ASSERT_TRUE(locationPath);

    EXPECT_THAT(eval(*namePath, doc), IsJsonString(u"Jayway Inc."));

    EXPECT_THAT(locationPath->evaluate(doc).value_or(QJsonArray{}),
                ElementsAre(IsJsonString(u"Malmo"), IsJsonString(u"San Francisco"), IsJsonString(u"Helsingborg")));
}

//---------------------------------------------
// 4.2  Predicates – expensive books > 20
//---------------------------------------------
TEST(JSONPathBaeldung, PredicatePriceGreaterThan20)
{
    static const char jsonSrc[] = R"JSON({
        "book": [
            {"title": "Beginning JSON", "author": "Ben Smith", "price": 49.99},
            {"title": "JSON at Work", "author": "Tom Marrs", "price": 29.99},
            {"title": "Learn JSON in a DAY", "author": "Acodemy", "price": 8.99},
            {"title": "JSON: Questions and Answers", "author": "George Duckett", "price": 6.00}
        ],
        "price range": {"cheap": 10.00, "medium": 20.00}
    })JSON";

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonSrc));

    // Inline predicate variant equivalent to article but comparing with constant
    auto expensive{JSONPath::create(u"$['book'][?(@.price > 20.00)]")};
    ASSERT_TRUE(expensive);
    auto result{expensive->evaluate(doc)};
    ASSERT_TRUE(result.has_value()) << "Failed to evaluate expensive books";
    QJsonArray  expensiveBooks = *result;
    QStringList titles;
    for (const auto& v : expensiveBooks)
        titles << v.toObject().value("title").toString();

    EXPECT_TRUE(titles.contains(u"Beginning JSON"_s));
    EXPECT_TRUE(titles.contains(u"JSON at Work"_s));
    EXPECT_FALSE(titles.contains(u"Learn JSON in a DAY"_s));
    EXPECT_FALSE(titles.contains(u"JSON: Questions and Answers"_s));
}

//---------------------------------------------
// 6.1  Getting Object Data Given IDs
//---------------------------------------------
TEST(JSONPathBaeldung, MovieWithId2)
{
    static const char jsonSrc[] = R"JSON([
        {"id": 1, "title": "Casino Royale", "director": "Martin Campbell", "starring": ["Daniel Craig", "Eva Green"], "desc": "Twenty-first James Bond movie", "release date": 1163466000000, "box office": 594275385},
        {"id": 2, "title": "Quantum of Solace", "director": "Marc Forster", "starring": ["Daniel Craig", "Olga Kurylenko"], "desc": "Twenty-second James Bond movie", "release date": 1225242000000, "box office": 591692078},
        {"id": 3, "title": "Skyfall", "director": "Sam Mendes", "starring": ["Daniel Craig", "Naomie Harris"], "desc": "Twenty-third James Bond movie", "release date": 1350954000000, "box office": 1110526981},
        {"id": 4, "title": "Spectre", "director": "Sam Mendes", "starring": ["Daniel Craig", "Lea Seydoux"], "desc": "Twenty-fourth James Bond movie", "release date": 1445821200000, "box office": 879376275}
    ])JSON";

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonSrc));

    auto path{JSONPath::create(u"$[?(@.id == 2)]")};
    ASSERT_TRUE(path);
    auto resResult{path->evaluate(doc)};
    ASSERT_TRUE(resResult.has_value()) << "Failed to evaluate path";
    QJsonArray resArr = *resResult;
    ASSERT_EQ(resArr.size(), 1);
    const QJsonObject obj = resArr[0].toObject();

    EXPECT_EQ(obj.value("id").toInt(), 2);
    EXPECT_EQ(obj.value("title").toString(), u"Quantum of Solace"_s);
    EXPECT_TRUE(obj.value("desc").toString().contains(u"Twenty-second James Bond movie"_s));
}

//---------------------------------------------
// 6.2  Movie Title Given Starring – 'in' operator
//---------------------------------------------
TEST(JSONPathBaeldung, TitleByStarringEvaGreen)
{
    static const char jsonSrc[] = R"JSON([
        {"id": 1, "title": "Casino Royale", "director": "Martin Campbell", "starring": ["Daniel Craig", "Eva Green"], "desc": "Twenty-first James Bond movie", "release date": 1163466000000, "box office": 594275385},
        {"id": 2, "title": "Quantum of Solace", "director": "Marc Forster", "starring": ["Daniel Craig", "Olga Kurylenko"], "desc": "Twenty-second James Bond movie", "release date": 1225242000000, "box office": 591692078},
        {"id": 3, "title": "Skyfall", "director": "Sam Mendes", "starring": ["Daniel Craig", "Naomie Harris"], "desc": "Twenty-third James Bond movie", "release date": 1350954000000, "box office": 1110526981},
        {"id": 4, "title": "Spectre", "director": "Sam Mendes", "starring": ["Daniel Craig", "Lea Seydoux"], "desc": "Twenty-fourth James Bond movie", "release date": 1445821200000, "box office": 879376275}
    ])JSON";

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonSrc));

    auto path{JSONPath::create(u"$[?('Eva Green' in @['starring'])]")};
    ASSERT_TRUE(path);
    auto resResult{path->evaluate(doc)};
    ASSERT_TRUE(resResult.has_value()) << "Failed to evaluate path";
    QJsonArray res = *resResult;
    ASSERT_EQ(res.size(), 1);
    QJsonObject obj = res[0].toObject();
    EXPECT_EQ(obj.value("title").toString(), u"Casino Royale"_s);
}

//---------------------------------------------
// 6.3  Calculation of the Total Revenue
//---------------------------------------------
TEST(JSONPathBaeldung, TotalRevenue)
{
    static const char jsonSrc[] = R"JSON([
        {"id": 1, "title": "Casino Royale", "director": "Martin Campbell", "starring": ["Daniel Craig", "Eva Green"], "desc": "Twenty-first James Bond movie", "release date": 1163466000000, "box office": 594275385},
        {"id": 2, "title": "Quantum of Solace", "director": "Marc Forster", "starring": ["Daniel Craig", "Olga Kurylenko"], "desc": "Twenty-second James Bond movie", "release date": 1225242000000, "box office": 591692078},
        {"id": 3, "title": "Skyfall", "director": "Sam Mendes", "starring": ["Daniel Craig", "Naomie Harris"], "desc": "Twenty-third James Bond movie", "release date": 1350954000000, "box office": 1110526981},
        {"id": 4, "title": "Spectre", "director": "Sam Mendes", "starring": ["Daniel Craig", "Lea Seydoux"], "desc": "Twenty-fourth James Bond movie", "release date": 1445821200000, "box office": 879376275}
    ])JSON";

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonSrc));

    // Equivalent to Java example: iterate through array and sum revenues
    auto root{JSONPath::create(u"$")};
    auto moviesResult{root->evaluate(doc)};
    ASSERT_TRUE(moviesResult.has_value()) << "Failed to evaluate root";
    QJsonArray movies  = *moviesResult;
    long long  revenue = 0;
    for (const auto& v : movies)
        revenue += v.toObject().value("box office").toVariant().toLongLong();

    EXPECT_EQ(revenue, 594275385LL + 591692078LL + 1110526981LL + 879376275LL);
}

//---------------------------------------------
// 6.4  Highest Revenue Movie
//---------------------------------------------
TEST(JSONPathBaeldung, HighestRevenueMovie)
{
    static const char jsonSrc[] = R"JSON([
        {"id": 1, "title": "Casino Royale", "director": "Martin Campbell", "starring": ["Daniel Craig", "Eva Green"], "desc": "Twenty-first James Bond movie", "release date": 1163466000000, "box office": 594275385},
        {"id": 2, "title": "Quantum of Solace", "director": "Marc Forster", "starring": ["Daniel Craig", "Olga Kurylenko"], "desc": "Twenty-second James Bond movie", "release date": 1225242000000, "box office": 591692078},
        {"id": 3, "title": "Skyfall", "director": "Sam Mendes", "starring": ["Daniel Craig", "Naomie Harris"], "desc": "Twenty-third James Bond movie", "release date": 1350954000000, "box office": 1110526981},
        {"id": 4, "title": "Spectre", "director": "Sam Mendes", "starring": ["Daniel Craig", "Lea Seydoux"], "desc": "Twenty-fourth James Bond movie", "release date": 1445821200000, "box office": 879376275}
    ])JSON";

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonSrc));

    // 1. Get list of revenues
    auto revenuePath{JSONPath::create(u"$[*]['box office']")};
    auto revenuesResult{revenuePath->evaluate(doc)};
    ASSERT_TRUE(revenuesResult.has_value()) << "Failed to evaluate revenues";
    QJsonArray revenues = *revenuesResult;
    long long  highest  = 0;
    for (const auto& v : revenues)
        highest = std::max(highest, v.toVariant().toLongLong());

    // 2. Find movie with that revenue
    auto moviePath{JSONPath::create(QString(u"$[?(@['box office'] == %1)]").arg(highest))};
    ASSERT_TRUE(moviePath);
    auto resResult{moviePath->evaluate(doc)};
    ASSERT_TRUE(resResult.has_value()) << "Failed to evaluate movie path";
    QJsonArray res = *resResult;
    ASSERT_EQ(res.size(), 1);
    EXPECT_EQ(res[0].toObject().value("title").toString(), u"Skyfall"_s);
}

//---------------------------------------------
// 6.5  Latest Movie of Director – logical && operator
//---------------------------------------------
TEST(JSONPathBaeldung, LatestMovieOfSamMendes)
{
    static const char jsonSrc[] = R"JSON([
        {"id": 1, "title": "Casino Royale", "director": "Martin Campbell", "starring": ["Daniel Craig", "Eva Green"], "desc": "Twenty-first James Bond movie", "release date": 1163466000000, "box office": 594275385},
        {"id": 2, "title": "Quantum of Solace", "director": "Marc Forster", "starring": ["Daniel Craig", "Olga Kurylenko"], "desc": "Twenty-second James Bond movie", "release date": 1225242000000, "box office": 591692078},
        {"id": 3, "title": "Skyfall", "director": "Sam Mendes", "starring": ["Daniel Craig", "Naomie Harris"], "desc": "Twenty-third James Bond movie", "release date": 1350954000000, "box office": 1110526981},
        {"id": 4, "title": "Spectre", "director": "Sam Mendes", "starring": ["Daniel Craig", "Lea Seydoux"], "desc": "Twenty-fourth James Bond movie", "release date": 1445821200000, "box office": 879376275}
    ])JSON";

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray(jsonSrc));

    auto path{JSONPath::create(u"$[?(@['director'] == 'Sam Mendes' && @['release date'] == 1445821200000)]")};
    ASSERT_TRUE(path);
    auto resResult{path->evaluate(doc)};
    ASSERT_TRUE(resResult.has_value()) << "Failed to evaluate path";
    QJsonArray res = *resResult;
    ASSERT_EQ(res.size(), 1);
    QJsonObject obj = res[0].toObject();
    EXPECT_EQ(obj.value("title").toString(), u"Spectre"_s);
}
