// JSONPathGTest.cpp - GoogleTest-based unit tests replacing QtTest suite
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "json-query/JSONPath.hpp"
#include "json-query/JSONPointer.hpp"

// Helper to evaluate a JSONPath on a doc and assert single value equality
static QJsonValue evalSingle(const JSONPath &path, const QJsonDocument &doc)
{
    const auto res = path.evaluate(doc);
    EXPECT_EQ(res.size(), 1);
    return res[0];
}

TEST(JSONPathBasic, RootAccess)
{
    QJsonObject obj{{"foo", "bar"}};
    QJsonDocument doc(obj);
    JSONPath path("$");
    ASSERT_TRUE(path.isValid());
    auto res = path.evaluate(doc);
    ASSERT_EQ(res.size(), 1);
    EXPECT_EQ(res[0].toObject(), obj);
}

TEST(JSONPathBasic, PropertyAccess)
{
    QJsonObject obj{{"foo", "bar"}, {"baz", 42}};
    QJsonDocument doc(obj);
    JSONPath path("$.foo");
    EXPECT_EQ(evalSingle(path, doc).toString(), QStringLiteral("bar"));
    JSONPath path2("$['baz']");
    EXPECT_EQ(evalSingle(path2, doc).toInt(), 42);
}

TEST(JSONPathBasic, ArrayIndex)
{
    QJsonArray arr = QJsonArray::fromVariantList({"zero", "one", "two"});
    QJsonDocument doc(arr);
    JSONPath idx("$[1]");
    EXPECT_EQ(evalSingle(idx, doc).toString(), QStringLiteral("one"));
    JSONPath neg("$[-1]");
    EXPECT_EQ(evalSingle(neg, doc).toString(), QStringLiteral("two"));
}

TEST(JSONPathPointerInterop, CompareWithJSONPointer)
{
    QJsonObject obj{{"items", QJsonArray{ QJsonObject{{"id", 1}, {"name", "Item1"}} }}};
    QJsonDocument doc(obj);
    JSONPointer ptr("/items/0/name");
    JSONPath path("$.items[0].name");
    EXPECT_EQ(evalSingle(path, doc), ptr.evaluate(doc));
}
