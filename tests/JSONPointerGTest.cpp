// JSONPointerGTest.cpp - GoogleTest port of functional JSONPointer tests
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include "../include/json-query/json-pointer/JSONPointer.hpp"
#include "framework/JSONMatchersGTest.hpp"

using json_query::JSONPointer;
using namespace ::testing;

static QJsonValue evalPtr(QStringView ptr, const QJsonDocument &doc)
{
    auto jp{JSONPointer::create(ptr)};
    EXPECT_TRUE(jp) << qPrintable(QStringLiteral("Pointer invalid: %1").arg(QString(ptr)));
    auto res{jp->evaluate(doc)};
    return res ? res.value() : QJsonValue{QJsonValue::Undefined};
}

TEST(JSONPointerBasic, EmptyPointer)
{
    QJsonDocument doc(QJsonObject{{"foo","bar"}});
    auto p{JSONPointer::create(QStringLiteral(""))};
    ASSERT_TRUE(p);
    auto valRes{p->evaluate(doc)};
    EXPECT_TRUE(valRes);
    EXPECT_THAT(valRes.value(), JsonObjContains(kvlist(kv("foo","bar"))));
}

TEST(JSONPointerBasic, ObjectAccess)
{
    QJsonObject obj{{"foo","bar"},{"baz",42}};
    QJsonDocument doc(obj);
    EXPECT_THAT(evalPtr(QStringLiteral("/foo"), doc), IsJsonString("bar"));
    EXPECT_THAT(evalPtr(QStringLiteral("/baz"), doc), IsJsonInt(42));
}

TEST(JSONPointerBasic, NestedObject)
{
    QJsonObject obj{{"nested", QJsonObject{{"inner","value"}}}};
    QJsonDocument doc(obj);
    EXPECT_THAT(evalPtr(QStringLiteral("/nested/inner"), doc), IsJsonString("value"));
}

TEST(JSONPointerArray, Index)
{
    QJsonArray arr = QJsonArray::fromStringList({"zero","one","two"});
    QJsonDocument doc(arr);
    EXPECT_THAT(evalPtr(QStringLiteral("/0"), doc), IsJsonString("zero"));
    EXPECT_THAT(evalPtr(QStringLiteral("/2"), doc), IsJsonString("two"));
}

TEST(JSONPointerArray, Mixed)
{
    QJsonDocument doc(QJsonObject{{"array", QJsonArray::fromStringList({"a","b","c"})}});
    EXPECT_THAT(evalPtr(QStringLiteral("/array/1"), doc), IsJsonString("b"));
}

TEST(JSONPointerEscaping, SlashTilde)
{
    QJsonObject obj{{"foo/bar","v1"},{"foo~bar","v2"}};
    QJsonDocument doc(obj);
    EXPECT_THAT(evalPtr(QStringLiteral("/foo~1bar"), doc), IsJsonString("v1"));
    EXPECT_THAT(evalPtr(QStringLiteral("/foo~0bar"), doc), IsJsonString("v2"));
}

TEST(JSONPointerError, InvalidPointer)
{
    auto invalid{JSONPointer::create(QStringLiteral("foo/bar"))};
    EXPECT_FALSE(invalid);
}

TEST(JSONPointerError, NonExistent)
{
    QJsonDocument doc(QJsonObject{{"foo","bar"}});
    EXPECT_TRUE(evalPtr(QStringLiteral("/missing"), doc).isUndefined());
    EXPECT_TRUE(evalPtr(QStringLiteral("/foo/0"), doc).isUndefined());
}
