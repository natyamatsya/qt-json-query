// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// JSONPointerConformanceGTest.cpp - GoogleTest port of RFC-6901 suite
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include "../../../include/json-query/json-pointer/JSONPointer.hpp"
#include "framework/JSONMatchersGTest.hpp"

using json_query::json_pointer::JSONPointer;
using namespace ::testing;

namespace
{

static const QJsonDocument& testDoc()
{
    static const QJsonDocument doc(
        []
        {
            QJsonObject obj{{"foo", QJsonArray{QStringLiteral("bar"), QStringLiteral("baz")}},
                            {"", 0},
                            {"a/b", 1},
                            {"c%d", 2},
                            {"e^f", 3},
                            {"g|h", 4},
                            {"i\\j", 5},
                            {"k\"l", 6},
                            {" ", 7},
                            {"m~n", 8}};
            return QJsonDocument(obj);
        }());
    return doc;
}

static void compareJson(const QJsonValue& actual, const QJsonValue& expected)
{
    if (expected.isObject())
    {
        EXPECT_EQ(QJsonDocument(actual.toObject()).toJson(QJsonDocument::Compact),
                  QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact));
    }
    else if (expected.isArray())
    {
        EXPECT_EQ(QJsonDocument(actual.toArray()).toJson(QJsonDocument::Compact),
                  QJsonDocument(expected.toArray()).toJson(QJsonDocument::Compact));
    }
    else
    {
        EXPECT_EQ(actual, expected);
    }
}

TEST(JSONPointerConformance, ValidPointers)
{
    const QJsonObject root = testDoc().object();
    struct Case
    {
        QString    ptr;
        QJsonValue expected;
    };
    const std::vector<Case> cases = {{QString{}, QJsonValue(root)},
                                     {"/foo", root.value("foo")},
                                     {"/foo/0", root.value("foo").toArray().at(0)},
                                     {"/", root.value("")},
                                     {"/a~1b", root.value("a/b")},
                                     {"/c%d", root.value("c%d")},
                                     {"/e^f", root.value("e^f")},
                                     {"/g|h", root.value("g|h")},
                                     {"/i\\j", root.value("i\\j")},
                                     {"/k\"l", root.value("k\"l")},
                                     {"/ ", root.value(" ")},
                                     {"/m~0n", root.value("m~n")}};

    for (const auto& c : cases)
    {
        auto jp{JSONPointer::create(c.ptr)};
        ASSERT_TRUE(jp) << qPrintable(QStringLiteral("Invalid pointer: %1").arg(c.ptr));
        auto res{jp->evaluate(testDoc())};
        ASSERT_TRUE(res);
        compareJson(res.value(), c.expected);
    }
}

TEST(JSONPointerConformance, InvalidPointers)
{
    const QStringList invalidPtrs{"foo/bar", "//", "/foo//bar"};
    for (const QString& ptr : invalidPtrs)
    {
        auto jp{JSONPointer::create(ptr)};
        ASSERT_FALSE(jp) << qPrintable(QStringLiteral("Pointer unexpectedly valid: %1").arg(ptr));
        // jp is error, nothing to evaluate
    }
}

} // namespace
