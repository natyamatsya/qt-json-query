// JSONPointerGTest.cpp - GoogleTest port of functional JSONPointer tests
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "json-query/JSONPointer.hpp"

using namespace ::testing;

static QJsonValue evalPtr(const QString &ptr, const QJsonDocument &doc)
{
    JSONPointer p(ptr);
    EXPECT_TRUE(p.isValid()) << qPrintable(QStringLiteral("Pointer invalid: %1").arg(ptr));
    return p.evaluate(doc);
}

TEST(JSONPointerBasic, EmptyPointer)
{
    QJsonDocument doc(QJsonObject{{"foo","bar"}});
    JSONPointer p("");
    ASSERT_TRUE(p.isValid());
    EXPECT_EQ(p.evaluate(doc).toObject(), doc.object());
}

TEST(JSONPointerBasic, ObjectAccess)
{
    QJsonObject obj{{"foo","bar"},{"baz",42}};
    QJsonDocument doc(obj);
    EXPECT_EQ(evalPtr("/foo", doc).toString(), "bar");
    EXPECT_EQ(evalPtr("/baz", doc).toInt(), 42);
}

TEST(JSONPointerBasic, NestedObject)
{
    QJsonObject obj{{"nested", QJsonObject{{"inner","value"}}}};
    QJsonDocument doc(obj);
    EXPECT_EQ(evalPtr("/nested/inner", doc).toString(), "value");
}

TEST(JSONPointerArray, Index)
{
    QJsonArray arr = QJsonArray::fromStringList({"zero","one","two"});
    QJsonDocument doc(arr);
    EXPECT_EQ(evalPtr("/0", doc).toString(), "zero");
    EXPECT_EQ(evalPtr("/2", doc).toString(), "two");
}

TEST(JSONPointerArray, Mixed)
{
    QJsonDocument doc(QJsonObject{{"array", QJsonArray::fromStringList({"a","b","c"})}});
    EXPECT_EQ(evalPtr("/array/1", doc).toString(), "b");
}

TEST(JSONPointerEscaping, SlashTilde)
{
    QJsonObject obj{{"foo/bar","v1"},{"foo~bar","v2"}};
    QJsonDocument doc(obj);
    EXPECT_EQ(evalPtr("/foo~1bar", doc).toString(), "v1");
    EXPECT_EQ(evalPtr("/foo~0bar", doc).toString(), "v2");
}

TEST(JSONPointerError, InvalidPointer)
{
    JSONPointer invalid("foo/bar");
    EXPECT_FALSE(invalid.isValid());
    EXPECT_TRUE(invalid.evaluate(QJsonDocument()).isUndefined());
}

TEST(JSONPointerError, NonExistent)
{
    QJsonDocument doc(QJsonObject{{"foo","bar"}});
    EXPECT_TRUE(evalPtr("/missing", doc).isUndefined());
    EXPECT_TRUE(evalPtr("/foo/0", doc).isUndefined());
}
