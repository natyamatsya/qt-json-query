// JSONPointerConformanceGTest.cpp - GoogleTest port of RFC-6901 suite
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include "json-query/JSONPointer.hpp"
#include "framework/JSONMatchersGTest.hpp"

using json_query::JSONPointer;
using namespace ::testing;

namespace {

static const QJsonDocument &testDoc()
{
    static const QJsonDocument doc([]{
        QJsonObject obj{
            {"foo", QJsonArray{QStringLiteral("bar"), QStringLiteral("baz")}},
            {"", 0},
            {"a/b", 1},
            {"c%d", 2},
            {"e^f", 3},
            {"g|h", 4},
            {"i\\j", 5},
            {"k\"l", 6},
            {" ", 7},
            {"m~n", 8}
        };
        return QJsonDocument(obj);
    }());
    return doc;
}

static void compareJson(const QJsonValue &actual, const QJsonValue &expected)
{
    if (expected.isObject()) {
        EXPECT_EQ(QJsonDocument(actual.toObject()).toJson(QJsonDocument::Compact),
                  QJsonDocument(expected.toObject()).toJson(QJsonDocument::Compact));
    } else if (expected.isArray()) {
        EXPECT_EQ(QJsonDocument(actual.toArray()).toJson(QJsonDocument::Compact),
                  QJsonDocument(expected.toArray()).toJson(QJsonDocument::Compact));
    } else {
        EXPECT_EQ(actual, expected);
    }
}

TEST(JSONPointerConformance, ValidPointers)
{
    const QJsonObject root = testDoc().object();
    struct Case { QString ptr; QJsonValue expected; };
    const std::vector<Case> cases = {
        {QString(), QJsonValue(root)},
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
        {"/m~0n", root.value("m~n")}
    };

    for (const auto &c : cases) {
        JSONPointer jp(c.ptr);
        ASSERT_TRUE(jp.isValid()) << qPrintable(QStringLiteral("Invalid pointer: %1").arg(c.ptr));
        compareJson(jp.evaluate(testDoc()), c.expected);
    }
}

TEST(JSONPointerConformance, InvalidPointers)
{
    const QStringList invalidPtrs{"foo/bar", "//", "/foo//bar"};
    for (const QString &ptr : invalidPtrs) {
        JSONPointer jp(ptr);
        ASSERT_FALSE(jp.isValid()) << qPrintable(QStringLiteral("Pointer unexpectedly valid: %1").arg(ptr));
        EXPECT_THAT(jp.evaluate(testDoc()), IsJsonUndefined());
    }
}

} // namespace
