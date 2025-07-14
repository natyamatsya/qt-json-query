// JSONPathGTest.cpp - comprehensive GoogleTest suite (formerly JSONPathTestGTest)
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "json-query/JSONPath.hpp"
#include "json-query/JSONPointer.hpp"

// Helper to evaluate a JSONPath on a doc and assert single value equality
static QJsonValue evalSingle(const JSONPath &path, const QJsonDocument &doc)
{
    const QJsonValue res = path.evaluate(doc);
    if (res.isArray()) {
        const auto arr = res.toArray();
        EXPECT_EQ(arr.size(), 1);
        return arr[0];
    }
    return res;
}

TEST(JSONPathBasic, RootAccess)
{
    QJsonObject obj{{"foo", "bar"}};
    QJsonDocument doc(obj);
    auto path{ JSONPath::create(u"$") };
    ASSERT_TRUE(path);
    QJsonValue res = path->evaluate(doc);
    if (res.isArray()) {
        ASSERT_EQ(res.toArray().size(), 1);
        res = res.toArray()[0];
    }
    EXPECT_EQ(res.toObject(), obj);
}

TEST(JSONPathBasic, PropertyAccess)
{
    QJsonObject obj{{"foo", "bar"}, {"baz", 42}};
    QJsonDocument doc(obj);
    auto path{ JSONPath::create(u"$.foo") };
    EXPECT_EQ(evalSingle(*path, doc).toString(), QStringLiteral("bar"));
    auto path2{ JSONPath::create(u"$['baz']") };
    EXPECT_EQ(evalSingle(*path2, doc).toInt(), 42);
}

TEST(JSONPathBasic, ArrayIndex)
{
    QJsonArray arr = QJsonArray::fromVariantList({"zero", "one", "two"});
    QJsonDocument doc(arr);
    auto idx{ JSONPath::create(u"$[1]") };
    EXPECT_EQ(evalSingle(*idx, doc).toString(), QStringLiteral("one"));
    auto neg{ JSONPath::create(u"$[-1]") };
    EXPECT_EQ(evalSingle(*neg, doc).toString(), QStringLiteral("two"));
}

TEST(JSONPathPointerInterop, CompareWithJSONPointer)
{
    QJsonObject obj{{"items", QJsonArray{ QJsonObject{{"id", 1}, {"name", "Item1"}} }}};
    QJsonDocument doc(obj);
    JSONPointer ptr("/items/0/name");
    auto path{ JSONPath::create(u"$.items[0].name") };
    EXPECT_EQ(evalSingle(*path, doc), ptr.evaluate(doc));
}
