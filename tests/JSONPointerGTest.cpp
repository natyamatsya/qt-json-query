// JSONPointerGTest.cpp - GoogleTest port of functional JSONPointer tests
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include "json-query/JSONPointer.hpp"
#include "framework/JSONMatchersGTest.hpp"

using json_query::JSONPointer;
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
    EXPECT_THAT(p.evaluate(doc), JsonObjContains(kvlist(kv("foo","bar"))));
}

TEST(JSONPointerBasic, ObjectAccess)
{
    QJsonObject obj{{"foo","bar"},{"baz",42}};
    QJsonDocument doc(obj);
    EXPECT_THAT(evalPtr("/foo", doc), IsJsonString("bar"));
    EXPECT_THAT(evalPtr("/baz", doc), IsJsonInt(42));
}

TEST(JSONPointerBasic, NestedObject)
{
    QJsonObject obj{{"nested", QJsonObject{{"inner","value"}}}};
    QJsonDocument doc(obj);
    EXPECT_THAT(evalPtr("/nested/inner", doc), IsJsonString("value"));
}

TEST(JSONPointerArray, Index)
{
    QJsonArray arr = QJsonArray::fromStringList({"zero","one","two"});
    QJsonDocument doc(arr);
    EXPECT_THAT(evalPtr("/0", doc), IsJsonString("zero"));
    EXPECT_THAT(evalPtr("/2", doc), IsJsonString("two"));
}

TEST(JSONPointerArray, Mixed)
{
    QJsonDocument doc(QJsonObject{{"array", QJsonArray::fromStringList({"a","b","c"})}});
    EXPECT_THAT(evalPtr("/array/1", doc), IsJsonString("b"));
}

TEST(JSONPointerEscaping, SlashTilde)
{
    QJsonObject obj{{"foo/bar","v1"},{"foo~bar","v2"}};
    QJsonDocument doc(obj);
    EXPECT_THAT(evalPtr("/foo~1bar", doc), IsJsonString("v1"));
    EXPECT_THAT(evalPtr("/foo~0bar", doc), IsJsonString("v2"));
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
