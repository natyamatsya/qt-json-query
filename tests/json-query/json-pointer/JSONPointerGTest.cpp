// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// JSONPointerGTest.cpp - GoogleTest port of functional JSONPointer tests
#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <gtest/gtest-spi.h>
#include <algorithm>
#include "framework/JSONMatchersGTest.hpp"
#include "json-query/JSONQuery" // For public API aliases

using json_query::json_pointer::JSONPointer;
using namespace ::testing;

static QJsonValue evalPtr(QStringView ptr, const QJsonDocument& doc)
{
    auto jp{JSONPointer::create(ptr)};
    EXPECT_TRUE(jp) << qPrintable(QStringLiteral("Pointer invalid: %1").arg(QString(ptr)));
    auto res{jp->evaluate(doc)};
    return res ? res.value() : QJsonValue{QJsonValue::Undefined};
}

TEST(JSONPointerBasic, EmptyPointer)
{
    QJsonDocument doc(QJsonObject{{"foo", "bar"}});
    auto          p{JSONPointer::create(QStringLiteral(""))};
    ASSERT_TRUE(p);
    auto valRes{p->evaluate(doc)};
    EXPECT_TRUE(valRes);
    EXPECT_THAT(valRes.value(), JsonObjContains(kvlist(kv("foo", "bar"))));
}

TEST(JSONPointerBasic, ObjectAccess)
{
    QJsonObject   obj{{"foo", "bar"}, {"baz", 42}};
    QJsonDocument doc(obj);
    EXPECT_THAT(evalPtr(QStringLiteral("/foo"), doc), IsJsonString("bar"));
    EXPECT_THAT(evalPtr(QStringLiteral("/baz"), doc), IsJsonInt(42));
}

TEST(JSONPointerBasic, NestedObject)
{
    QJsonObject   obj{{"nested", QJsonObject{{"inner", "value"}}}};
    QJsonDocument doc(obj);
    EXPECT_THAT(evalPtr(QStringLiteral("/nested/inner"), doc), IsJsonString("value"));
}

TEST(JSONPointerArray, Index)
{
    QJsonArray    arr = QJsonArray::fromStringList({"zero", "one", "two"});
    QJsonDocument doc(arr);
    EXPECT_THAT(evalPtr(QStringLiteral("/0"), doc), IsJsonString("zero"));
    EXPECT_THAT(evalPtr(QStringLiteral("/2"), doc), IsJsonString("two"));
}

TEST(JSONPointerArray, Mixed)
{
    QJsonDocument doc(QJsonObject{{"array", QJsonArray::fromStringList({"a", "b", "c"})}});
    EXPECT_THAT(evalPtr(QStringLiteral("/array/1"), doc), IsJsonString("b"));
}

TEST(JSONPointerEscaping, SlashTilde)
{
    QJsonObject   obj{{"foo/bar", "v1"}, {"foo~bar", "v2"}};
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
    QJsonDocument doc(QJsonObject{{"foo", "bar"}});
    EXPECT_TRUE(evalPtr(QStringLiteral("/missing"), doc).isUndefined());
    EXPECT_TRUE(evalPtr(QStringLiteral("/foo/0"), doc).isUndefined());
}

// ---------------------------------------------------------------------------
// Composition: appended() / operator/
// ---------------------------------------------------------------------------

TEST(JSONPointerComposition, AppendedNavigatesLikeParsed)
{
    const QJsonDocument doc(QJsonObject{
        {"interfaces", QJsonArray{QJsonObject{{"mac", "aa"}}, QJsonObject{{"mac", "bb"}}}}});

    const auto base{JSONPointer::create(QStringLiteral("/interfaces")).value()};
    for (qsizetype i = 0; i < 2; ++i)
    {
        const auto composed{base / i / u"mac"};
        // Identical to the parsed equivalent, string and behavior
        const auto parsed{JSONPointer::create(QStringLiteral("/interfaces/%1/mac").arg(i)).value()};
        EXPECT_EQ(composed.to_string(), parsed.to_string());
        auto r{composed.evaluate(doc)};
        ASSERT_TRUE(r.has_value());
        EXPECT_EQ(*r, parsed.evaluate(doc).value());
    }
}

TEST(JSONPointerComposition, KeysEnterAsDataNeverThroughTheParser)
{
    // Keys with '/' and '~' need no escaping at the appended() call site and
    // re-encode canonically in to_string()
    const auto p{JSONPointer::create(u"").value().appended(u"a/b").appended(u"m~n")};
    EXPECT_EQ(p.to_string(), QStringLiteral("/a~1b/m~0n"));

    const QJsonDocument doc(QJsonObject{{"a/b", QJsonObject{{"m~n", 1}}}});
    auto                r{p.evaluate(doc)};
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, QJsonValue{1});
}

TEST(JSONPointerComposition, NumericKeyClassifiedLikeParser)
{
    // "5" composes as an array index (usable on arrays AND as object member
    // "5", per container-relative semantics) — same as a parsed pointer
    const auto p{JSONPointer::create(u"/arr").value() / u"5"};
    const QJsonDocument arrDoc(QJsonObject{{"arr", QJsonArray{0, 1, 2, 3, 4, 50}}});
    EXPECT_EQ(p.evaluate(arrDoc).value(), QJsonValue{50});
    const QJsonDocument objDoc(QJsonObject{{"arr", QJsonObject{{"5", "x"}}}});
    EXPECT_EQ(p.evaluate(objDoc).value(), QJsonValue{QStringLiteral("x")});
}

TEST(JSONPointerComposition, DashAndNegativeIndex)
{
    // "-" composes as the append designator for writes
    QJsonDocument doc(QJsonObject{{"arr", QJsonArray{1}}});
    const auto    appendPtr{JSONPointer::create(u"/arr").value() / u"-"};
    ASSERT_TRUE(appendPtr.add(doc, 2).has_value());
    EXPECT_EQ(doc.object().value("arr").toArray(), (QJsonArray{1, 2}));

    // Negative appended(qsizetype) is a member name (container-relative)
    const auto neg{JSONPointer::create(u"").value() / qsizetype{-1}};
    EXPECT_EQ(neg.to_string(), QStringLiteral("/-1"));
    const QJsonDocument objDoc(QJsonObject{{"-1", "m"}});
    EXPECT_EQ(neg.evaluate(objDoc).value(), QJsonValue{QStringLiteral("m")});
}
