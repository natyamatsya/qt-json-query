// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// JSONPatchGTest.cpp - Unit tests for the JSONPatch module. Complements the
// json-patch-tests compliance suite with checks the community data cannot
// express: error domains/codes, operation index in Error::detail, atomicity
// of applyInPlace, and the numeric edge of the test-op equality.
#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <string>
#include "json-query/JSONQuery"

using json_query::Error;
using json_query::ErrorDomain;
using json_query::JSONPatch;

namespace
{

QJsonArray parsePatchJson(const char* json)
{
    QJsonParseError perr{};
    const auto      doc{QJsonDocument::fromJson(QByteArray{json}, &perr)};
    EXPECT_EQ(perr.error, QJsonParseError::NoError) << perr.errorString().toStdString();
    EXPECT_TRUE(doc.isArray()) << "test patch must be a JSON array";
    const QJsonArray patch = doc.array();
    // Guard the guard: every op entry must round-trip as an object before the
    // patch is handed to create() (diagnoses container-level mishaps directly)
    for (qsizetype i = 0; i < patch.size(); ++i)
        EXPECT_TRUE(patch.at(i).isObject())
            << "op " << i << " is not an object; array: "
            << QJsonDocument{patch}.toJson(QJsonDocument::Compact).toStdString();
    return patch;
}

// Failure formatter: what create()/apply() actually returned, for assertion
// messages (numeric() is high-byte domain / low-byte code, ADR-004)
template <class Expected>
std::string describe(const Expected& result)
{
    if (result.has_value())
        return "success";
    const auto& e{result.error()};
    return std::string{"error 0x"} + QString::number(e.numeric(), 16).toStdString() + " '" +
           std::string{e.message()} + "' at detail " + std::to_string(e.detail);
}

// ---------------------------------------------------------------------------
// create(): eager validation with op index in Error::detail
// ---------------------------------------------------------------------------

TEST(JSONPatchCreate, RejectsUnknownOperationWithOpIndex)
{
    const QJsonArray patch = parsePatchJson(R"([
        {"op": "add", "path": "/a", "value": 1},
        {"op": "frobnicate", "path": "/b"}
    ])");
    auto result{JSONPatch::create(patch)};
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().domain, ErrorDomain::PatchParse) << describe(result);
    EXPECT_EQ(result.error().detail, 1) << describe(result); // second operation
}

TEST(JSONPatchCreate, RejectsMissingValueForAddReplaceTest)
{
    for (const char* op : {"add", "replace", "test"})
    {
        const QJsonArray patch = QJsonArray{QJsonObject{{"op", op}, {"path", "/a"}}};
        EXPECT_FALSE(JSONPatch::create(patch).has_value()) << op;
    }
}

TEST(JSONPatchCreate, AcceptsExplicitNullValue)
{
    // "value": null must NOT be treated as a missing member
    const QJsonArray patch = parsePatchJson(R"([{"op": "add", "path": "/a", "value": null}])");
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

    QJsonDocument doc(QJsonObject{});
    ASSERT_TRUE(compiled->applyInPlace(doc).has_value());
    EXPECT_TRUE(doc.object().contains(QStringLiteral("a")));
    EXPECT_TRUE(doc.object().value(QStringLiteral("a")).isNull());
}

TEST(JSONPatchCreate, RejectsMissingFromForMoveCopy)
{
    for (const char* op : {"move", "copy"})
    {
        const QJsonArray patch = QJsonArray{QJsonObject{{"op", op}, {"path", "/a"}}};
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
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);
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
    const QJsonArray patch = parsePatchJson(R"([
        {"op": "add",     "path": "/a", "value": 1},
        {"op": "add",     "path": "/b", "value": 2},
        {"op": "replace", "path": "/missing", "value": 3},
        {"op": "add",     "path": "/c", "value": 4}
    ])");
    auto compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

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
    const QJsonArray patch = parsePatchJson(R"([
        {"op": "test", "path": "/a", "value": 1},
        {"op": "test", "path": "/a", "value": 999}
    ])");
    auto compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

    const QJsonDocument doc(QJsonObject{{"a", 1}});
    auto                result{compiled->apply(doc)};
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().domain, ErrorDomain::PatchEval);
    EXPECT_EQ(result.error().detail, 1);
}

TEST(JSONPatchApply, MoveIntoOwnDescendantFails)
{
    const QJsonArray patch = parsePatchJson(R"([{"op": "move", "from": "/a", "path": "/a/b"}])");
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

    const QJsonDocument doc(QJsonObject{{"a", QJsonObject{{"b", 1}}}});
    auto                result{compiled->apply(doc)};
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().domain, ErrorDomain::PatchEval);
}

TEST(JSONPatchApply, CopyIntoOwnDescendantIsAllowed)
{
    // RFC 6902 forbids moving into a descendant, not copying: the source
    // value is snapshotted before the add
    const QJsonArray patch = parsePatchJson(R"([{"op": "copy", "from": "/a", "path": "/a/b"}])");
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

    const QJsonDocument doc(QJsonObject{{"a", QJsonObject{{"x", 1}}}});
    auto                result{compiled->apply(doc)};
    ASSERT_TRUE(result.has_value());
    const auto a{result->object().value(QStringLiteral("a")).toObject()};
    EXPECT_EQ(a.value(QStringLiteral("x")), QJsonValue{1});
    EXPECT_EQ(a.value(QStringLiteral("b")).toObject(), (QJsonObject{{"x", 1}}));
}

TEST(JSONPatchApply, MoveToSameLocationIsIdentity)
{
    // from == path is not a *proper* prefix, so the move is legal; per the
    // spec it is remove-then-add at the same location — an identity
    const QJsonArray patch = parsePatchJson(R"([
        {"op": "move", "from": "/a", "path": "/a"},
        {"op": "move", "from": "/arr/1", "path": "/arr/1"}
    ])");
    auto compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

    const QJsonDocument doc(QJsonObject{{"a", 1}, {"arr", QJsonArray{1, 2, 3}}});
    auto                result{compiled->apply(doc)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, doc);
}

TEST(JSONPatchApply, TestAgainstRootPointer)
{
    const QJsonArray matching = parsePatchJson(R"([{"op": "test", "path": "", "value": {"a": 1}}])");
    const auto doc{QJsonDocument(QJsonObject{{"a", 1}})};
    ASSERT_TRUE(JSONPatch::create(matching)->apply(doc).has_value());

    const QJsonArray mismatching = parsePatchJson(R"([{"op": "test", "path": "", "value": {"a": 2}}])");
    auto       result{JSONPatch::create(mismatching)->apply(doc)};
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().domain, ErrorDomain::PatchEval);
}

TEST(JSONPatchApply, MoveToSiblingWithSharedNamePrefixIsAllowed)
{
    // "/ab" is NOT a descendant of "/a" — the prefix check must respect
    // token boundaries
    const QJsonArray patch = parsePatchJson(R"([{"op": "move", "from": "/a", "path": "/ab"}])");
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

    const QJsonDocument doc(QJsonObject{{"a", 1}});
    auto                result{compiled->apply(doc)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->object(), (QJsonObject{{"ab", 1}}));
}

TEST(JSONPatchApply, TestComparesNumbersNumerically)
{
    // Qt distinguishes integer and double representations; RFC 6902 §4.6
    // compares numbers numerically, so 1 must equal 1.0
    const QJsonArray patch = parsePatchJson(R"([{"op": "test", "path": "/n", "value": 1}])");
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

    QJsonObject obj;
    obj.insert(QStringLiteral("n"), QJsonValue{1.0}); // stored as double
    EXPECT_TRUE(compiled->apply(QJsonDocument(obj)).has_value());
}

TEST(JSONPatchApply, TestDoesNotCoerceAcrossTypes)
{
    const QJsonArray patch = parsePatchJson(R"([{"op": "test", "path": "/n", "value": "1"}])");
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);
    EXPECT_FALSE(compiled->apply(QJsonDocument(QJsonObject{{"n", 1}})).has_value());
}

TEST(JSONPatchApply, CopyThenMoveComposition)
{
    const QJsonArray patch = parsePatchJson(R"([
        {"op": "copy", "from": "/src", "path": "/dup"},
        {"op": "move", "from": "/src", "path": "/dst"}
    ])");
    auto compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

    const QJsonDocument doc(QJsonObject{{"src", QJsonArray{1, 2}}});
    auto                result{compiled->apply(doc)};
    ASSERT_TRUE(result.has_value());
    const auto obj{result->object()};
    EXPECT_FALSE(obj.contains(QStringLiteral("src")));
    EXPECT_EQ(obj.value(QStringLiteral("dup")).toArray(), (QJsonArray{1, 2}));
    EXPECT_EQ(obj.value(QStringLiteral("dst")).toArray(), (QJsonArray{1, 2}));
}

TEST(JSONPatchApply, ApplyInPlaceTypedRoots)
{
    const QJsonArray patch = parsePatchJson(R"([{"op": "add", "path": "/b", "value": 2}])");
    auto             compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

    QJsonObject obj{{"a", 1}};
    ASSERT_TRUE(compiled->applyInPlace(obj).has_value());
    EXPECT_EQ(obj, (QJsonObject{{"a", 1}, {"b", 2}}));

    // Root-kind change under a typed root is RootTypeMismatch, root untouched
    const QJsonArray kindChange = parsePatchJson(R"([{"op": "add", "path": "", "value": [1]}])");
    auto             changer{JSONPatch::create(kindChange)};
    ASSERT_TRUE(changer.has_value()) << describe(changer);
    const QJsonObject before{obj};
    auto              r{changer->applyInPlace(obj)};
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().domain, ErrorDomain::PointerEval);
    EXPECT_EQ(obj, before);

    QJsonArray       arr{1, 2};
    const QJsonArray arrPatchOps = parsePatchJson(R"([{"op": "remove", "path": "/0"}])");
    auto             arrPatch{JSONPatch::create(arrPatchOps)};
    ASSERT_TRUE(arrPatch.has_value());
    ASSERT_TRUE(arrPatch->applyInPlace(arr).has_value());
    EXPECT_EQ(arr, (QJsonArray{2}));
}

TEST(JSONPatchApply, ScalarRootResultNeedsValueOverload)
{
    const QJsonArray patch = parsePatchJson(R"([{"op": "add", "path": "", "value": 42}])");
    auto       compiled{JSONPatch::create(patch)};
    ASSERT_TRUE(compiled.has_value()) << describe(compiled);

    // Document overload cannot represent a scalar root
    EXPECT_FALSE(compiled->apply(QJsonDocument(QJsonObject{})).has_value());

    // Value overload can
    auto viaValue{compiled->apply(QJsonValue{QJsonObject{}})};
    ASSERT_TRUE(viaValue.has_value());
    EXPECT_EQ(*viaValue, QJsonValue{42});
}

} // anonymous namespace
