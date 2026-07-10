// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// JSONPointerWriteGTest.cpp - Unit tests for JSONPointer write operations
// (add/replace/remove/set). Complements the data-driven suite in
// tests/rfc-compliance-suite/rfc-6901/rfc6901-write-tests.json with checks the
// JSON case format cannot express: error codes and token indices in
// Error::detail, QJsonDocument root edge cases, Undefined normalization, and
// COW isolation of independent copies.
#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "json-query/JSONQuery"

using json_query::Error;
using json_query::ErrorDomain;
using json_query::json_pointer::EvalError;
using json_query::json_pointer::JSONPointer;
using json_query::json_pointer::WriteOptions;

namespace
{

JSONPointer ptr(QStringView s)
{
    auto p{JSONPointer::create(s)};
    EXPECT_TRUE(p.has_value()) << qPrintable(QStringLiteral("Pointer invalid: %1").arg(QString(s)));
    return *p;
}

void expectError(const Error& err, EvalError code, std::uint16_t tokenIndex)
{
    EXPECT_EQ(err.domain, ErrorDomain::PointerEval);
    EXPECT_EQ(err.code, static_cast<std::uint8_t>(code));
    EXPECT_EQ(err.detail, tokenIndex);
}

// ---------------------------------------------------------------------------
// Error codes and failing token index (Error::detail)
// ---------------------------------------------------------------------------

TEST(JSONPointerWriteError, RemoveRootIsCannotRemoveRoot)
{
    QJsonDocument doc(QJsonObject{{"a", 1}});
    auto          r{ptr(u"").remove(doc)};
    ASSERT_FALSE(r.has_value());
    expectError(r.error(), EvalError::CannotRemoveRoot, 0);
}

TEST(JSONPointerWriteError, ReplaceMissingKeyReportsTokenIndex)
{
    QJsonDocument doc(QJsonObject{{"a", QJsonObject{{"b", 1}}}});
    auto          r{ptr(u"/a/missing").replace(doc, 2)};
    ASSERT_FALSE(r.has_value());
    expectError(r.error(), EvalError::KeyNotFound, 1);
}

TEST(JSONPointerWriteError, MissingParentReportsTokenIndex)
{
    QJsonDocument doc(QJsonObject{{"a", 1}});
    auto          r{ptr(u"/x/y/z").add(doc, 1)};
    ASSERT_FALSE(r.has_value());
    expectError(r.error(), EvalError::KeyNotFound, 0);
}

TEST(JSONPointerWriteError, IndexPastEndReportsIndexOutOfRange)
{
    QJsonDocument doc(QJsonObject{{"arr", QJsonArray{1, 2}}});
    auto          r{ptr(u"/arr/5").add(doc, 3)};
    ASSERT_FALSE(r.has_value());
    expectError(r.error(), EvalError::IndexOutOfRange, 1);
}

TEST(JSONPointerWriteError, DashOnReplaceReportsIndexOutOfRange)
{
    QJsonDocument doc(QJsonObject{{"arr", QJsonArray{1, 2}}});
    auto          r{ptr(u"/arr/-").replace(doc, 3)};
    ASSERT_FALSE(r.has_value());
    expectError(r.error(), EvalError::IndexOutOfRange, 1);
}

TEST(JSONPointerWriteError, ScalarInPathReportsTypeMismatchObject)
{
    QJsonDocument doc(QJsonObject{{"a", "scalar"}});
    auto          r{ptr(u"/a/b").add(doc, 1)};
    ASSERT_FALSE(r.has_value());
    expectError(r.error(), EvalError::TypeMismatchObject, 1);
}

TEST(JSONPointerWriteError, NonIndexTokenOnArrayReportsTypeMismatchArray)
{
    QJsonDocument doc(QJsonObject{{"arr", QJsonArray{1, 2}}});
    auto          r{ptr(u"/arr/name").add(doc, 3)};
    ASSERT_FALSE(r.has_value());
    expectError(r.error(), EvalError::TypeMismatchArray, 1);
}

// ---------------------------------------------------------------------------
// QJsonDocument root edge cases
// ---------------------------------------------------------------------------

TEST(JSONPointerWriteRoot, ScalarRootOnDocumentIsError)
{
    // QJsonDocument can only hold an object or array root; replacing the root
    // with a scalar is representable only via the QJsonValue overload.
    QJsonDocument       doc(QJsonObject{{"a", 1}});
    const QJsonDocument before{doc};
    auto                r{ptr(u"").add(doc, QStringLiteral("scalar"))};
    ASSERT_FALSE(r.has_value());
    expectError(r.error(), EvalError::DocumentRootNotContainer, 0);
    EXPECT_EQ(doc, before); // strong guarantee
}

TEST(JSONPointerWriteRoot, ScalarRootOnValueOverloadWorks)
{
    QJsonValue root{QJsonObject{{"a", 1}}};
    ASSERT_TRUE(ptr(u"").add(root, QStringLiteral("scalar")).has_value());
    EXPECT_EQ(root, QJsonValue{QStringLiteral("scalar")});
}

TEST(JSONPointerWriteRoot, NullRootOnDocumentGivesNullDocument)
{
    QJsonDocument doc(QJsonObject{{"a", 1}});
    ASSERT_TRUE(ptr(u"").add(doc, QJsonValue{}).has_value());
    EXPECT_TRUE(doc.isNull());
}

TEST(JSONPointerWriteRoot, SetWithCreateBootstrapsEmptyDocument)
{
    // The settings-backend shape: write a deep path into a default document.
    QJsonDocument doc;
    ASSERT_TRUE(ptr(u"/ui/theme/accent").set(doc, QStringLiteral("teal"), {.createIntermediates = true}).has_value());
    const QJsonDocument expected(
        QJsonObject{{"ui", QJsonObject{{"theme", QJsonObject{{"accent", "teal"}}}}}});
    EXPECT_EQ(doc, expected);
}

TEST(JSONPointerWriteRoot, RootReplaceSwapsContainerKind)
{
    QJsonDocument doc(QJsonObject{{"a", 1}});
    ASSERT_TRUE(ptr(u"").replace(doc, QJsonArray{1, 2}).has_value());
    EXPECT_TRUE(doc.isArray());
    EXPECT_EQ(doc.array(), (QJsonArray{1, 2}));
}

// ---------------------------------------------------------------------------
// Value normalization and returned values
// ---------------------------------------------------------------------------

TEST(JSONPointerWriteValues, UndefinedValueIsStoredAsNull)
{
    // Inserting Undefined into a QJsonObject would remove the member; the
    // write API normalizes it to JSON null instead.
    QJsonDocument doc(QJsonObject{{"a", 1}});
    ASSERT_TRUE(ptr(u"/a").replace(doc, QJsonValue{QJsonValue::Undefined}).has_value());
    EXPECT_TRUE(doc.object().contains(QStringLiteral("a")));
    EXPECT_TRUE(doc.object().value(QStringLiteral("a")).isNull());
}

TEST(JSONPointerWriteValues, RemoveReturnsRemovedValue)
{
    QJsonDocument doc(QJsonObject{{"a", QJsonObject{{"b", QJsonArray{10, 20}}}}});
    auto          r{ptr(u"/a/b/0").remove(doc)};
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, QJsonValue{10});
    EXPECT_EQ(doc.object().value("a").toObject().value("b").toArray(), (QJsonArray{20}));
}

// ---------------------------------------------------------------------------
// COW isolation
// ---------------------------------------------------------------------------

TEST(JSONPointerWriteCow, PreWriteCopiesAreIndependent)
{
    QJsonDocument       doc(QJsonObject{{"deep", QJsonObject{{"list", QJsonArray{1, 2, 3}}}}});
    const QJsonDocument snapshot{doc};
    const QJsonValue    sharedChild{doc.object().value("deep")};

    ASSERT_TRUE(ptr(u"/deep/list/1").replace(doc, 99).has_value());

    // The write rebuilt the spine; every pre-write handle still sees the old data.
    EXPECT_EQ(snapshot.object().value("deep").toObject().value("list").toArray(), (QJsonArray{1, 2, 3}));
    EXPECT_EQ(sharedChild.toObject().value("list").toArray(), (QJsonArray{1, 2, 3}));
    EXPECT_EQ(doc.object().value("deep").toObject().value("list").toArray(), (QJsonArray{1, 99, 3}));
}

TEST(JSONPointerWriteCow, UntouchedSiblingsCompareEqualAfterWrite)
{
    QJsonObject   big;
    for (int i = 0; i < 50; ++i)
        big.insert(QStringLiteral("key%1").arg(i), QJsonArray{i, i + 1});
    big.insert(QStringLiteral("target"), QJsonObject{{"x", 0}});
    QJsonDocument doc(big);

    ASSERT_TRUE(ptr(u"/target/x").replace(doc, 1).has_value());
    for (int i = 0; i < 50; ++i)
        EXPECT_EQ(doc.object().value(QStringLiteral("key%1").arg(i)).toArray(), (QJsonArray{i, i + 1}));
    EXPECT_EQ(doc.object().value("target").toObject().value("x"), QJsonValue{1});
}

// ---------------------------------------------------------------------------
// Typed-root overloads (QJsonObject& / QJsonArray&)
// ---------------------------------------------------------------------------

TEST(JSONPointerWriteTypedRoot, ObjectRootWritesInPlace)
{
    QJsonObject root{{"a", QJsonObject{{"b", 1}}}};
    ASSERT_TRUE(ptr(u"/a/b").replace(root, 2).has_value());
    ASSERT_TRUE(ptr(u"/c").set(root, 3, {.createIntermediates = true}).has_value());
    EXPECT_EQ(root.value("a").toObject().value("b"), QJsonValue{2});
    EXPECT_EQ(root.value("c"), QJsonValue{3});

    auto removed{ptr(u"/a").remove(root)};
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(removed->toObject(), (QJsonObject{{"b", 2}}));
    EXPECT_FALSE(root.contains(QStringLiteral("a")));
}

TEST(JSONPointerWriteTypedRoot, ArrayRootWritesInPlace)
{
    QJsonArray root{1, 2};
    ASSERT_TRUE(ptr(u"/-").add(root, 3).has_value());
    EXPECT_EQ(root, (QJsonArray{1, 2, 3}));

    auto removed{ptr(u"/0").remove(root)};
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(*removed, QJsonValue{1});
    EXPECT_EQ(root, (QJsonArray{2, 3}));
}

TEST(JSONPointerWriteTypedRoot, RootReplaceWithMatchingKindWorks)
{
    QJsonObject root{{"old", 1}};
    ASSERT_TRUE(ptr(u"").replace(root, QJsonObject{{"new", 2}}).has_value());
    EXPECT_EQ(root, (QJsonObject{{"new", 2}}));
}

TEST(JSONPointerWriteTypedRoot, RootKindChangeIsRootTypeMismatch)
{
    QJsonObject       root{{"a", 1}};
    const QJsonObject before{root};

    auto r{ptr(u"").replace(root, QJsonArray{1, 2})};
    ASSERT_FALSE(r.has_value());
    expectError(r.error(), EvalError::RootTypeMismatch, 0);
    EXPECT_EQ(root, before); // strong guarantee

    QJsonArray       arr{1};
    const QJsonArray arrBefore{arr};
    auto             r2{ptr(u"").add(arr, QJsonObject{{"a", 1}})};
    ASSERT_FALSE(r2.has_value());
    expectError(r2.error(), EvalError::RootTypeMismatch, 0);
    EXPECT_EQ(arr, arrBefore);
}

TEST(JSONPointerWriteTypedRoot, FailedWriteLeavesTypedRootUntouched)
{
    QJsonObject       root{{"a", 1}};
    const QJsonObject before{root};
    ASSERT_FALSE(ptr(u"/x/y").add(root, 1).has_value());
    EXPECT_EQ(root, before);
}

// ---------------------------------------------------------------------------
// set() / createIntermediates specifics
// ---------------------------------------------------------------------------

TEST(JSONPointerWriteSet, DefaultOptionsEqualAdd)
{
    QJsonDocument docSet(QJsonObject{{"a", 1}});
    QJsonDocument docAdd(QJsonObject{{"a", 1}});
    ASSERT_TRUE(ptr(u"/b").set(docSet, 2).has_value());
    ASSERT_TRUE(ptr(u"/b").add(docAdd, 2).has_value());
    EXPECT_EQ(docSet, docAdd);

    auto failing{ptr(u"/x/y").set(docSet, 1)};
    ASSERT_FALSE(failing.has_value());
    expectError(failing.error(), EvalError::KeyNotFound, 0);
}

TEST(JSONPointerWriteSet, CreateIntermediatesNeverOverwritesScalar)
{
    QJsonDocument       doc(QJsonObject{{"a", 42}});
    const QJsonDocument before{doc};
    auto                r{ptr(u"/a/b/c").set(doc, 1, {.createIntermediates = true})};
    ASSERT_FALSE(r.has_value());
    expectError(r.error(), EvalError::TypeMismatchObject, 1);
    EXPECT_EQ(doc, before);
}

TEST(JSONPointerWriteSet, CreatedArrayRejectsGapIndex)
{
    QJsonDocument doc(QJsonObject{});
    auto          r{ptr(u"/a/2").set(doc, 1, {.createIntermediates = true})};
    // "/a" creates an object (next token "2" is a nonzero index → object per
    // the next-token rule), so this succeeds as an object member...
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(doc.object().value("a").toObject().value("2"), QJsonValue{1});

    // ...but a gap index into a freshly created *array* (next token 0 → array,
    // then index 2 at the leaf) fails without null-padding.
    QJsonDocument doc2(QJsonObject{{"a", QJsonArray{}}});
    auto          r2{ptr(u"/a/2").set(doc2, 1, {.createIntermediates = true})};
    ASSERT_FALSE(r2.has_value());
    expectError(r2.error(), EvalError::IndexOutOfRange, 1);
}

} // anonymous namespace
