// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// JSONPathGTest.cpp - comprehensive GoogleTest suite (formerly JSONPathTestGTest)
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "framework/JSONMatchersGTest.hpp"
#include "../include/json-query/json-pointer/JSONPointer.hpp"

// Using declarations for convenience
using json_query::JSONPath;
using json_query::JSONPointer;

TEST(JSONPathBasic, RootAccess)
{
    QJsonObject   obj{{"foo", "bar"}};
    QJsonDocument doc(obj);
    auto          path{JSONPath::create(u"$")};
    ASSERT_TRUE(path);
    QJsonArray res = evalArray(*path, doc);
    ASSERT_EQ(res.size(), 1);
    EXPECT_EQ(res[0].toObject(), obj);
}

TEST(JSONPathBasic, PropertyAccess)
{
    QJsonObject   obj{{"foo", "bar"}, {"baz", 42}};
    QJsonDocument doc(obj);
    EXPECT_THAT(eval(u"$.foo", doc), IsJsonString(u"bar"));
    EXPECT_THAT(eval(u"$['baz']", doc), IsJsonInt(42));
}

TEST(JSONPathBasic, ArrayIndex)
{
    QJsonArray    arr = QJsonArray::fromVariantList({"zero", "one", "two"});
    QJsonDocument doc(arr);
    EXPECT_THAT(eval(u"$[1]", doc), IsJsonString(u"one"));
    EXPECT_THAT(eval(u"$[-1]", doc), IsJsonString(u"two"));
}

TEST(JSONPathPointerInterop, CompareWithJSONPointer)
{
    QJsonObject   obj{{"items", QJsonArray{QJsonObject{{"id", 1}, {"name", "Item1"}}}}};
    QJsonDocument doc(obj);
    auto          ptr{JSONPointer::create(QStringLiteral("/items/0/name"))};
    ASSERT_TRUE(ptr);
    EXPECT_EQ(eval(u"$.items[0].name", doc), ptr->evaluate(doc));
}
