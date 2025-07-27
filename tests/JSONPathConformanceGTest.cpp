// JSONPathConformanceGTest.cpp - GoogleTest version of minimal conformance suite
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "framework/JSONMatchersGTest.hpp"

// Using declarations for convenience
using json_query::JSONPath;

namespace {

static const QJsonDocument &sampleDoc()
{
    // Same store example used previously
    static const QJsonDocument doc([]{
        QJsonObject bicycle{{"color","red"},{"price",19.95}};
        QJsonArray books{
            QJsonObject{{"category","reference"},{"author","Nigel Rees"},{"title","Sayings of the Century"},{"price",8.95}},
            QJsonObject{{"category","fiction"},{"author","Evelyn Waugh"},{"title","Sword of Honour"},{"price",12.99}},
            QJsonObject{{"category","fiction"},{"author","Herman Melville"},{"title","Moby Dick"},{"isbn","0-553-21311-3"},{"price",8.99}},
            QJsonObject{{"category","fiction"},{"author","J. R. R. Tolkien"},{"title","The Lord of the Rings"},{"isbn","0-395-19395-8"},{"price",22.99}}
        };
        QJsonObject store{{"book",books},{"bicycle",bicycle}};
        QJsonObject root{{"store",store},{"expensive",10}};
        return QJsonDocument(root);
    }());
    return doc;
}

static void compareJson(const QJsonValue &actual, const QJsonValue &expected)
{
    if (expected.isArray()) {
        EXPECT_EQ(QJsonDocument(actual.toArray()).toJson(QJsonDocument::Compact),
                  QJsonDocument(expected.toArray()).toJson(QJsonDocument::Compact));
    } else if (expected.isObject()) {
        EXPECT_EQ(QJsonDocument(actual.toObject()).toJson(QJsonDocument::Compact),
                  QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact));
    } else {
        EXPECT_EQ(actual, expected);
    }
}

TEST(JSONPathConformance, ValidPaths)
{
    const auto root{sampleDoc().object()};
    const auto books{root["store"].toObject()["book"].toArray()};

    struct Case { QString path; QJsonValue expected; };
    const std::vector<Case> cases = {
        {"$", QJsonValue(root)},
        {"$.store.book[0].author", QJsonValue("Nigel Rees")},
        {"$['store']['bicycle']['color']", QJsonValue("red")},
        {"$.store.book[*].price", QJsonValue::fromVariant(QVariantList{8.95,12.99,8.99,22.99})},
        {"$.store.book[?(@.price > 20)].title", QJsonValue::fromVariant(QVariantList{"The Lord of the Rings"})},
        {"$..isbn", QJsonValue::fromVariant(QVariantList{"0-553-21311-3","0-395-19395-8"})}
    };

    for (const auto &c : cases) {
        auto jp { JSONPath::create(c.path) };
        ASSERT_TRUE(jp) << qPrintable(QStringLiteral("Invalid path: %1").arg(c.path));
        compareJson(eval(*jp, sampleDoc()), c.expected);
    }
}

} // namespace
