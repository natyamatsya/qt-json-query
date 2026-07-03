// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// JSONPathGTest.cpp - comprehensive GoogleTest suite (formerly JSONPathTestGTest)
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <gtest/gtest-spi.h>
#include <algorithm>
#include "framework/JSONMatchersGTest.hpp"
#include "json-query/JSONQuery" // For public API aliases

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

// Recursive descent must scale with document size — no arbitrary width,
// depth, or result-count limits (regression tests for the former
// kMaxStackDepth/kMaxResults caps that failed on nodes with >100 children).

TEST(JSONPathRecursiveDescentScale, WideNode)
{
    QJsonArray arr;
    for (int i = 0; i < 500; ++i)
        arr.append(i);
    QJsonDocument doc(arr);

    auto path{JSONPath::create(u"$..*")};
    ASSERT_TRUE(path);
    auto result{path->evaluate(doc)};
    ASSERT_TRUE(result) << "wide node must not fail recursive descent";
    EXPECT_EQ(result->size(), 500);
}

TEST(JSONPathRecursiveDescentScale, ManyResults)
{
    // 200 objects x (1 object + 1 array + 100 ints) = 20400 descendants
    QJsonArray root;
    for (int i = 0; i < 200; ++i)
    {
        QJsonArray inner;
        for (int j = 0; j < 100; ++j)
            inner.append(j);
        root.append(QJsonObject{{"a", inner}});
    }
    QJsonDocument doc(root);

    auto path{JSONPath::create(u"$..*")};
    ASSERT_TRUE(path);
    auto result{path->evaluate(doc)};
    ASSERT_TRUE(result) << "large result set must not fail recursive descent";
    EXPECT_EQ(result->size(), 200 * 102);
}

// RFC 9535: equal values at distinct locations are distinct nodes — recursive
// descent must not deduplicate containers by value (regression test for the
// former deduplicateJsonValues post-processing).

TEST(JSONPathRecursiveDescentScale, DuplicateContainersPreservedWildcard)
{
    // Three identical objects, each holding an identical array
    QJsonArray inner{0, 1};
    QJsonArray root{QJsonObject{{"a", inner}}, QJsonObject{{"a", inner}}, QJsonObject{{"a", inner}}};
    QJsonDocument doc(root);

    auto path{JSONPath::create(u"$..*")};
    ASSERT_TRUE(path);
    auto result{path->evaluate(doc)};
    ASSERT_TRUE(result);
    // 3 objects + 3 arrays + 6 ints
    EXPECT_EQ(result->size(), 12);
}

TEST(JSONPathRecursiveDescentScale, DuplicateContainersPreservedKey)
{
    QJsonObject   obj{{"x", QJsonObject{{"v", QJsonObject{}}}}, {"y", QJsonObject{{"v", QJsonObject{}}}}};
    QJsonDocument doc(obj);

    auto path{JSONPath::create(u"$..v")};
    ASSERT_TRUE(path);
    auto result{path->evaluate(doc)};
    ASSERT_TRUE(result);
    EXPECT_EQ(result->size(), 2); // both (equal-valued) nodes must be kept
}

TEST(JSONPathRecursiveDescentScale, DeepNesting)
{
    // 1000 nesting levels — matches Qt's own parser depth allowance order
    QJsonValue leaf{1};
    QJsonValue v{leaf};
    for (int i = 0; i < 1000; ++i)
        v = QJsonObject{{"a", v}};
    QJsonDocument doc(v.toObject());

    auto path{JSONPath::create(u"$..*")};
    ASSERT_TRUE(path);
    auto result{path->evaluate(doc)};
    ASSERT_TRUE(result) << "deep nesting must not fail recursive descent";
    EXPECT_EQ(result->size(), 1000);
}
