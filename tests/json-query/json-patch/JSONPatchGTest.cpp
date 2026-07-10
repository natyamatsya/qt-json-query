// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// JSONPatchGTest.cpp - Unit tests for the JSONPatch module. Complements the
// json-patch-tests compliance suite with checks the community data cannot
// express: error domains/codes, operation index in Error::detail, atomicity
// of applyInPlace, and the numeric edge of the test-op equality.
#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "json-query/JSONQuery"

using json_query::Error;
using json_query::ErrorDomain;
using json_query::JSONPatch;

namespace
{

QJsonArray parsePatchJson(const char* json)
{
    const auto doc{QJsonDocument::fromJson(json)};
    EXPECT_TRUE(doc.isArray()) << "test patch must be a JSON array";
    return doc.array();
}

// ---------------------------------------------------------------------------
// create(): eager validation with op index in Error::detail
// ---------------------------------------------------------------------------

TEST(JSONPatchCreate, RejectsUnknownOperationWithOpIndex)
{
    const auto patch{parsePatchJson(R"([
        {"op": "add", "path": "/a", "value": 1},
        {"op": "frobnicate", "path": "/b"}
    ])")};
    auto result{JSONPatch::create(patch)};
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().domain, ErrorDomain::PatchParse);
    EXPECT_EQ(result.error().detail, 1); // second operation
}

TEST(JSONPatchCreate, RejectsMissingValueForAddReplaceTest)
{
    for (const char* op : {"add", "replace", "test"})
    {
        const auto patch{QJsonArray{QJsonObject{{"op", op}, {"path", "/a"}}}};
        EXPECT_FALSE(JSONPatch::create(patch).has_value()) << op;
    }
}

TEST(JSONPatchCreate, AcceptsExplicitNullValue)
{
    // "value": null must NOT be treated as a missing member
    const auto patch{parsePatchJson(R"([{"op": "add", "path": "/a", "value": null}])")};
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value());

    QJsonDocument doc(QJsonObject{});
    ASSERT_TRUE(compiled->applyInPlace(doc).has_value());
    EXPECT_TRUE(doc.object().contains(QStringLiteral("a")));
    EXPECT_TRUE(doc.object().value(QStringLiteral("a")).isNull());
}

TEST(JSONPatchCreate, RejectsMissingFromForMoveCopy)
{
    for (const char* op : {"move", "copy"})
    {
        const auto patch{QJsonArray{QJsonObject{{"op", op}, {"path", "/a"}}}};
        EXPECT_FALSE(JSONPatch::create(patch).has_value()) << op;
    }
}

TEST(JSONPatchCreate, RejectsNonArrayDocument)
{
    const QJsonDocument objDoc(QJsonObject{{"op", "add"}});
    EXPECT_FALSE(JSONPatch::create(objDoc).has_value());
}

TEST(JSONPatchCreate, EmptyPatchIsValidNoOp)
{
    auto compiled{JSONPatch::create(QJsonArray{})};
    ASSERT_TRUE(compiled.has_value());
    EXPECT_TRUE(compiled->isEmpty());
    EXPECT_EQ(compiled->size(), 0);

    QJsonDocument doc(QJsonObject{{"a", 1}});
    auto          result{compiled->apply(doc)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, doc);
}

// ---------------------------------------------------------------------------
// apply(): atomicity and error reporting
// ---------------------------------------------------------------------------

TEST(JSONPatchApply, AtomicityFailingMidPatchLeavesInputUntouched)
{
    const auto patch{parsePatchJson(R"([
        {"op": "add",     "path": "/a", "value": 1},
        {"op": "add",     "path": "/b", "value": 2},
        {"op": "replace", "path": "/missing", "value": 3},
        {"op": "add",     "path": "/c", "value": 4}
    ])")};
    auto compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value());

    QJsonDocument       doc(QJsonObject{{"orig", true}});
    const QJsonDocument before{doc};

    auto applied{compiled->apply(doc)};
    ASSERT_FALSE(applied.has_value());
    EXPECT_EQ(doc, before) << "apply() must not touch its input";

    auto inPlace{compiled->applyInPlace(doc)};
    ASSERT_FALSE(inPlace.has_value());
    EXPECT_EQ(doc, before) << "applyInPlace() must be all-or-nothing";

    // The failing op index (2) travels in Error::detail with the underlying
    // pointer error domain/code intact
    EXPECT_EQ(inPlace.error().domain, ErrorDomain::PointerEval);
    EXPECT_EQ(inPlace.error().detail, 2);
}

TEST(JSONPatchApply, TestFailureReportsPatchEvalWithOpIndex)
{
    const auto patch{parsePatchJson(R"([
        {"op": "test", "path": "/a", "value": 1},
        {"op": "test", "path": "/a", "value": 999}
    ])")};
    auto compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value());

    const QJsonDocument doc(QJsonObject{{"a", 1}});
    auto                result{compiled->apply(doc)};
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().domain, ErrorDomain::PatchEval);
    EXPECT_EQ(result.error().detail, 1);
}

TEST(JSONPatchApply, MoveIntoOwnDescendantFails)
{
    const auto patch{parsePatchJson(R"([{"op": "move", "from": "/a", "path": "/a/b"}])")};
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value());

    const QJsonDocument doc(QJsonObject{{"a", QJsonObject{{"b", 1}}}});
    auto                result{compiled->apply(doc)};
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().domain, ErrorDomain::PatchEval);
}

TEST(JSONPatchApply, MoveToSiblingWithSharedNamePrefixIsAllowed)
{
    // "/ab" is NOT a descendant of "/a" — the prefix check must respect
    // token boundaries
    const auto patch{parsePatchJson(R"([{"op": "move", "from": "/a", "path": "/ab"}])")};
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value());

    const QJsonDocument doc(QJsonObject{{"a", 1}});
    auto                result{compiled->apply(doc)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->object(), (QJsonObject{{"ab", 1}}));
}

TEST(JSONPatchApply, TestComparesNumbersNumerically)
{
    // Qt distinguishes integer and double representations; RFC 6902 §4.6
    // compares numbers numerically, so 1 must equal 1.0
    const auto patch{parsePatchJson(R"([{"op": "test", "path": "/n", "value": 1}])")};
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value());

    QJsonObject obj;
    obj.insert(QStringLiteral("n"), QJsonValue{1.0}); // stored as double
    EXPECT_TRUE(compiled->apply(QJsonDocument(obj)).has_value());
}

TEST(JSONPatchApply, TestDoesNotCoerceAcrossTypes)
{
    const auto patch{parsePatchJson(R"([{"op": "test", "path": "/n", "value": "1"}])")};
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value());
    EXPECT_FALSE(compiled->apply(QJsonDocument(QJsonObject{{"n", 1}})).has_value());
}

TEST(JSONPatchApply, CopyThenMoveComposition)
{
    const auto patch{parsePatchJson(R"([
        {"op": "copy", "from": "/src", "path": "/dup"},
        {"op": "move", "from": "/src", "path": "/dst"}
    ])")};
    auto compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value());

    const QJsonDocument doc(QJsonObject{{"src", QJsonArray{1, 2}}});
    auto                result{compiled->apply(doc)};
    ASSERT_TRUE(result.has_value());
    const auto obj{result->object()};
    EXPECT_FALSE(obj.contains(QStringLiteral("src")));
    EXPECT_EQ(obj.value(QStringLiteral("dup")).toArray(), (QJsonArray{1, 2}));
    EXPECT_EQ(obj.value(QStringLiteral("dst")).toArray(), (QJsonArray{1, 2}));
}

TEST(JSONPatchApply, ScalarRootResultNeedsValueOverload)
{
    const auto patch{parsePatchJson(R"([{"op": "add", "path": "", "value": 42}])")};
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value());

    // Document overload cannot represent a scalar root
    EXPECT_FALSE(compiled->apply(QJsonDocument(QJsonObject{})).has_value());

    // Value overload can
    auto viaValue{compiled->apply(QJsonValue{QJsonObject{}})};
    ASSERT_TRUE(viaValue.has_value());
    EXPECT_EQ(*viaValue, QJsonValue{42});
}

} // anonymous namespace
