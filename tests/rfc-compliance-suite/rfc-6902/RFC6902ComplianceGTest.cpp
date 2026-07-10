// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// RFC6902ComplianceGTest.cpp - Parameterised Google-Test runner for the
// community JSON Patch compliance suite (compliance/json-patch-tests,
// github.com/json-patch/json-patch-tests: tests.json + spec_tests.json).
// -----------------------------------------------------------------------------
// Case semantics per the suite's README:
//   "expected" present -> apply must succeed and produce exactly that document
//   "error" present    -> the patch must be rejected (at create() or apply())
//   neither            -> apply must succeed (result unchecked)
//   "disabled": true   -> skipped during collection
// Every case runs against the QJsonValue overload (which can represent any
// root) and, where the document is a container, additionally against the
// QJsonDocument overload and applyInPlace() — all three must agree. Atomicity
// is checked on every path: on error the input compares equal to its
// pre-apply state.
// KnownFailures.hpp tracks justified gaps with the self-cleaning contract used
// by the IETF JSON Schema suite (a stale entry fails the suite).
// -----------------------------------------------------------------------------
#include <gtest/gtest.h>
#include <ostream>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "KnownFailures.hpp"
#include "json-query/JSONQuery"

using json_query::json_patch::JSONPatch;

namespace
{

struct RFC6902TestCase
{
    QString    file;    // source file name (tests.json / spec_tests.json)
    int        index{}; // index within the file
    QString    comment;
    QJsonValue document;
    QJsonValue patch;    // usually an array; kept as-is to exercise rejection
    QJsonValue expected; // Undefined when the case only checks success/failure
    bool       expectError{false};
    bool       hasExpected{false};
};

inline void PrintTo(const RFC6902TestCase& tc, std::ostream* os)
{
    *os << "RFC6902TestCase{file='" << tc.file.toStdString() << "', index=" << tc.index << ", comment='"
        << tc.comment.toStdString() << "', expectError=" << (tc.expectError ? "true" : "false") << "}";
}

static QList<RFC6902TestCase> loadPatchTestFile(const QString& path, const QString& fileName)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning("Failed to open JSON Patch test file %s", qPrintable(path));
        return {};
    }

    QJsonParseError perr;
    const auto      doc{QJsonDocument::fromJson(f.readAll(), &perr)};
    if (perr.error != QJsonParseError::NoError || !doc.isArray())
    {
        qWarning("Invalid JSON in JSON Patch test file %s: %s", qPrintable(path), qPrintable(perr.errorString()));
        return {};
    }

    QList<RFC6902TestCase> out;
    const QJsonArray       cases = doc.array();
    for (int i = 0; i < cases.size(); ++i)
    {
        const QJsonObject caseObj = cases.at(i).toObject();
        if (caseObj.value(QLatin1String("disabled")).toBool(false))
            continue;

        RFC6902TestCase tc;
        tc.file        = fileName;
        tc.index       = i;
        tc.comment     = caseObj.value(QLatin1String("comment")).toString();
        tc.document    = caseObj.value(QLatin1String("doc"));
        tc.patch       = caseObj.value(QLatin1String("patch"));
        tc.expectError = caseObj.contains(QLatin1String("error"));
        tc.hasExpected = caseObj.contains(QLatin1String("expected"));
        if (tc.hasExpected)
            tc.expected = caseObj.value(QLatin1String("expected"));
        out.append(std::move(tc));
    }
    return out;
}

// RFC 6902 §4.6 value equality for comparing results against the suite's
// "expected" documents: numbers compare numerically. QJsonValue::operator==
// distinguishes Qt's integer and double representations, which is stricter
// than the spec and could turn a representation difference introduced by a
// future submodule bump into a confusing false failure.
static bool jsonSpecEquals(const QJsonValue& a, const QJsonValue& b)
{
    if (a.isDouble() && b.isDouble())
        return a.toDouble() == b.toDouble();
    if (a.type() != b.type())
        return false;
    if (a.isObject())
    {
        const QJsonObject objA = a.toObject();
        const QJsonObject objB = b.toObject();
        if (objA.size() != objB.size())
            return false;
        for (auto it = objA.constBegin(); it != objA.constEnd(); ++it)
        {
            const auto itB{objB.constFind(it.key())};
            if (itB == objB.constEnd() || !jsonSpecEquals(it.value(), itB.value()))
                return false;
        }
        return true;
    }
    if (a.isArray())
    {
        const QJsonArray arrA = a.toArray();
        const QJsonArray arrB = b.toArray();
        if (arrA.size() != arrB.size())
            return false;
        for (qsizetype i = 0; i < arrA.size(); ++i)
            if (!jsonSpecEquals(arrA.at(i), arrB.at(i)))
                return false;
        return true;
    }
    return a == b;
}

static QJsonDocument toDocument(const QJsonValue& v)
{
    return v.isArray() ? QJsonDocument{v.toArray()} : QJsonDocument{v.toObject()};
}

#ifndef JSON_QUERY_SOURCE_DIR
#define JSON_QUERY_SOURCE_DIR "."
#endif

static QList<RFC6902TestCase> collectAllPatchCases()
{
    const QString base{QStringLiteral(JSON_QUERY_SOURCE_DIR "/compliance/json-patch-tests/")};
    QList<RFC6902TestCase> all;
    all += loadPatchTestFile(base + QStringLiteral("spec_tests.json"), QStringLiteral("spec_tests.json"));
    all += loadPatchTestFile(base + QStringLiteral("tests.json"), QStringLiteral("tests.json"));
    return all;
}

class RFC6902JsonPatchTest : public ::testing::TestWithParam<RFC6902TestCase>
{
};

TEST_P(RFC6902JsonPatchTest, AppliesPerSpec)
{
    const RFC6902TestCase& tc = GetParam();
    const bool knownFailure{
        rfc6902_compliance::matchesKnownFailure(rfc6902_compliance::kKnownFailures, tc.file, tc.index, tc.comment)};

    // ---- run: create, then apply against the QJsonValue overload ----------
    bool       ok{false};
    QJsonValue result{QJsonValue::Undefined};
    if (tc.patch.isArray())
    {
        if (auto patch{JSONPatch::create(tc.patch.toArray())})
        {
            const QJsonValue before{tc.document};
            auto             applied{patch->apply(tc.document)};
            ok = applied.has_value();
            if (ok)
                result = *applied;
            // Atomicity note: apply() is functional, the input QJsonValue is
            // untouched by construction; verify anyway.
            ASSERT_EQ(tc.document, before) << "apply() must not mutate its input";
        }
    }
    // A non-array "patch" member can only satisfy an error expectation
    // (ok stays false).

    // ---- QJsonDocument overload + applyInPlace must agree -----------------
    if (tc.patch.isArray() && (tc.document.isObject() || tc.document.isArray()))
    {
        if (auto patch{JSONPatch::create(tc.patch.toArray())})
        {
            QJsonDocument       doc{toDocument(tc.document)};
            const QJsonDocument before{doc};

            const auto docApplied{patch->apply(doc)};
            EXPECT_EQ(doc, before) << "apply(QJsonDocument) must not mutate its input";

            const auto inPlace{patch->applyInPlace(doc)};
            EXPECT_EQ(docApplied.has_value(), inPlace.has_value());
            if (!inPlace.has_value())
                EXPECT_EQ(doc, before) << "applyInPlace() must be all-or-nothing";

            if (ok && (result.isObject() || result.isArray()))
            {
                // Container-root results must be representable and identical
                // on the document path
                ASSERT_TRUE(docApplied.has_value())
                    << "document overload failed where the value overload succeeded: " << tc.comment.toStdString();
                EXPECT_EQ(*docApplied, toDocument(result));
                EXPECT_EQ(doc, toDocument(result));
            }
            else if (!ok)
            {
                EXPECT_FALSE(docApplied.has_value())
                    << "document overload succeeded where the value overload failed: " << tc.comment.toStdString();
            }
            // ok with a null/scalar root: representable only via the value
            // overload (the document overload reports DocumentRootNotContainer
            // or yields a null document) — nothing further to compare.
        }
    }

    const bool passed{tc.expectError ? !ok : (ok && (!tc.hasExpected || jsonSpecEquals(result, tc.expected)))};

    if (!passed && knownFailure)
        GTEST_SKIP() << "Known tracked failure: " << tc.comment.toStdString();
    if (passed && knownFailure)
        FAIL() << "Stale KnownFailures.hpp entry — this case now passes; remove it: " << tc.file.toStdString()
               << " index " << tc.index;

    if (tc.expectError)
    {
        EXPECT_FALSE(ok) << "Patch should have been rejected: " << tc.comment.toStdString();
        return;
    }

    ASSERT_TRUE(ok) << "Patch application failed: " << tc.comment.toStdString();
    if (tc.hasExpected)
    {
        const auto dump = [](const QJsonValue& v)
        {
            const QJsonDocument d{v.isArray() ? QJsonDocument{v.toArray()} : QJsonDocument{v.toObject()}};
            return d.toJson(QJsonDocument::Compact).toStdString();
        };
        EXPECT_TRUE(jsonSpecEquals(result, tc.expected))
            << "Result mismatch for '" << tc.comment.toStdString() << "'\n  actual:   " << dump(result)
            << "\n  expected: " << dump(tc.expected);
    }
}

static QList<RFC6902TestCase> g_allPatchCases = collectAllPatchCases();

// A missing/unparseable compliance/json-patch-tests submodule must be a red
// build, not a silently empty suite (the pinned data currently holds 108
// enabled cases; adjust the floor deliberately on a submodule bump).
TEST(RFC6902SuiteIntegrity, CasesWereCollected) { EXPECT_GE(g_allPatchCases.size(), 100); }

INSTANTIATE_TEST_SUITE_P(RFC6902_Compliance,
                         RFC6902JsonPatchTest,
                         ::testing::ValuesIn(g_allPatchCases),
                         [](const ::testing::TestParamInfo<RFC6902TestCase>& info)
                         {
                             QString n = info.param.file;
                             n.remove(QStringLiteral(".json"));
                             n += QStringLiteral("_");
                             QString c = info.param.comment.left(40);
                             for (auto& ch : c)
                                 if (!ch.isLetterOrNumber())
                                     ch = QChar('_');
                             n += c + QStringLiteral("_") + QString::number(info.index);
                             return n.toStdString();
                         });

} // anonymous namespace
