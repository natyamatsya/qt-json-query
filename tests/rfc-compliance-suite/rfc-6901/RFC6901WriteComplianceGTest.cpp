// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// RFC6901WriteComplianceGTest.cpp - Parameterised Google-Test runner for the
// JSON Pointer write-operation suite (rfc6901-write-tests.json).
// -----------------------------------------------------------------------------
// Exercises JSONPointer::add/replace/remove/set with RFC 6902 §4 operation
// semantics (cases named rfc6902_a* mirror the corresponding RFC 6902
// Appendix A examples). Every case runs against BOTH the QJsonDocument and the
// QJsonValue overloads and additionally asserts:
//   1. The strong guarantee: a failing write leaves the document untouched.
//   2. COW isolation: a copy taken before a successful write is unaffected.
//   3. remove() returns the removed value (expected_removed).
// -----------------------------------------------------------------------------
#include <gtest/gtest.h>
#include <ostream>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "framework/ComplianceDataGTest.hpp"
#include "json-query/JSONQuery"

using json_query::json_pointer::JSONPointer;
using json_query::json_pointer::WriteOptions;

namespace
{

struct RFC6901WriteTestCase
{
    QString    name;
    QString    op;      // "add" | "replace" | "remove" | "set"
    QString    pointer;
    QJsonValue document;
    QJsonValue value;
    QJsonValue expectedDocument;
    QJsonValue expectedRemoved;       // Undefined when the case does not check it
    bool       createIntermediates{false};
    bool       shouldFail{false};
};

inline void PrintTo(const RFC6901WriteTestCase& tc, std::ostream* os)
{
    *os << "RFC6901WriteTestCase{name='" << tc.name.toStdString() << "', op='" << tc.op.toStdString()
        << "', pointer='" << tc.pointer.toStdString() << "', shouldFail=" << (tc.shouldFail ? "true" : "false")
        << "}";
}

static QList<RFC6901WriteTestCase> loadWriteTestFile(const QString& path)
{
    const auto doc{compliance_framework::loadComplianceJson(path)};
    if (doc.isNull())
        return {};

    QList<RFC6901WriteTestCase> out;
    const QJsonArray            tests = doc.object().value("tests").toArray();
    for (const QJsonValue& vCase : tests)
    {
        const QJsonObject    tcObj = vCase.toObject();
        RFC6901WriteTestCase tc;
        tc.name               = tcObj.value("name").toString();
        tc.op                 = tcObj.value("op").toString();
        tc.pointer            = tcObj.value("pointer").toString();
        tc.document           = tcObj.value("document");
        tc.value              = tcObj.value("value"); // Undefined for remove
        tc.createIntermediates = tcObj.value("create_intermediates").toBool(false);
        tc.shouldFail         = tcObj.value("should_fail").toBool(false);
        if (!tc.shouldFail)
        {
            tc.expectedDocument = tcObj.value("expected_document");
            tc.expectedRemoved  = tcObj.value("expected_removed"); // may stay Undefined
        }
        out.append(std::move(tc));
    }
    return out;
}

#ifndef JSON_QUERY_SOURCE_DIR
#define JSON_QUERY_SOURCE_DIR "."
#endif

static QList<RFC6901WriteTestCase> collectAllWriteCases()
{
    return loadWriteTestFile(
        QStringLiteral(JSON_QUERY_SOURCE_DIR "/tests/rfc-compliance-suite/rfc-6901/rfc6901-write-tests.json"));
}

static QJsonDocument toDocument(const QJsonValue& v)
{
    if (v.isArray())
        return QJsonDocument(v.toArray());
    return QJsonDocument(v.toObject());
}

class RFC6901JsonPointerWriteTest : public ::testing::TestWithParam<RFC6901WriteTestCase>
{
};

struct OpOutcome
{
    bool       ok{false};
    QJsonValue removed{QJsonValue::Undefined};
};

// One dispatch for every overload family the case runs against
// (QJsonDocument&, QJsonValue&, QJsonObject&, QJsonArray&)
template <class Target>
OpOutcome runOp(const JSONPointer& pointer, const RFC6901WriteTestCase& tc, Target& target, const WriteOptions& options)
{
    OpOutcome out;
    if (tc.op == QLatin1String("add"))
        out.ok = pointer.add(target, tc.value).has_value();
    else if (tc.op == QLatin1String("replace"))
        out.ok = pointer.replace(target, tc.value).has_value();
    else if (tc.op == QLatin1String("remove"))
    {
        auto r{pointer.remove(target)};
        out.ok = r.has_value();
        if (out.ok)
            out.removed = *r;
    }
    else if (tc.op == QLatin1String("set"))
        out.ok = pointer.set(target, tc.value, options).has_value();
    else
        ADD_FAILURE() << "Unknown op '" << tc.op.toStdString() << "' in test " << tc.name.toStdString();
    return out;
}

// Shared assertions: strong guarantee on failure, expected document + COW
// isolation + removed value on success. toValue converts Target -> QJsonValue
// for comparison against the case's JSON expectations.
template <class Target, class ToValue>
void checkOutcome(const RFC6901WriteTestCase& tc,
                  const OpOutcome&            out,
                  const Target&               target,
                  const Target&               before,
                  ToValue                     toValue,
                  const char*                 overloadName)
{
    if (tc.shouldFail)
    {
        EXPECT_FALSE(out.ok) << overloadName << ": write should have failed: " << tc.name.toStdString();
        EXPECT_EQ(target, before) << overloadName << ": failed write must leave the target untouched (strong "
                                  << "guarantee): " << tc.name.toStdString();
        return;
    }

    ASSERT_TRUE(out.ok) << overloadName << ": write failed unexpectedly: " << tc.name.toStdString();
    EXPECT_EQ(toValue(target), tc.expectedDocument)
        << overloadName << ": document mismatch for " << tc.name.toStdString();
    EXPECT_EQ(toValue(before), tc.document)
        << overloadName << ": pre-write copy must be unaffected (COW isolation): " << tc.name.toStdString();
    if (!tc.expectedRemoved.isUndefined())
        EXPECT_EQ(out.removed, tc.expectedRemoved)
            << overloadName << ": remove() returned the wrong value: " << tc.name.toStdString();
}

TEST_P(RFC6901JsonPointerWriteTest, WritesPerSpec)
{
    const RFC6901WriteTestCase& tc = GetParam();

    ASSERT_TRUE(tc.document.isObject() || tc.document.isArray())
        << "Write test document must be an object or array: " << tc.name.toStdString();

    auto maybePointer{JSONPointer::create(tc.pointer)};
    ASSERT_TRUE(maybePointer.has_value()) << "Failed to compile pointer: " << tc.pointer.toStdString();
    const JSONPointer& pointer = *maybePointer;

    const WriteOptions options{.createIntermediates = tc.createIntermediates};

    // ---- QJsonDocument overload -------------------------------------------
    {
        QJsonDocument       doc{toDocument(tc.document)};
        const QJsonDocument before{doc};
        const auto          out{runOp(pointer, tc, doc, options)};
        const auto          toValue = [](const QJsonDocument& d)
        { return d.isArray() ? QJsonValue{d.array()} : QJsonValue{d.object()}; };
        checkOutcome(tc, out, doc, before, toValue, "QJsonDocument");
    }

    // ---- QJsonValue overload ----------------------------------------------
    {
        QJsonValue       root{tc.document};
        const QJsonValue before{root};
        const auto       out{runOp(pointer, tc, root, options)};
        checkOutcome(tc, out, root, before, [](const QJsonValue& v) { return v; }, "QJsonValue");
    }

    // ---- Typed-root overloads ----------------------------------------------
    // The typed overload matching the document runs every case; the only
    // divergence from the QJsonValue overload is a successful root write that
    // CHANGES the root kind — under a fixed-kind root that must fail with
    // RootTypeMismatch and leave the target untouched.
    const bool kindChanges{!tc.shouldFail && tc.expectedDocument.type() != tc.document.type()};
    if (tc.document.isObject())
    {
        // Copy-init, not brace-init (ADR-001)
        QJsonObject       root = tc.document.toObject();
        const QJsonObject before = root;
        const auto        out{runOp(pointer, tc, root, options)};
        if (kindChanges)
        {
            EXPECT_FALSE(out.ok) << "QJsonObject: root-kind change must be RootTypeMismatch";
            EXPECT_EQ(root, before);
        }
        else
            checkOutcome(tc, out, root, before, [](const QJsonObject& o) { return QJsonValue{o}; }, "QJsonObject");
    }
    else
    {
        // Copy-init, not brace-init (ADR-001)
        QJsonArray       root = tc.document.toArray();
        const QJsonArray before = root;
        const auto       out{runOp(pointer, tc, root, options)};
        if (kindChanges)
        {
            EXPECT_FALSE(out.ok) << "QJsonArray: root-kind change must be RootTypeMismatch";
            EXPECT_EQ(root, before);
        }
        else
            checkOutcome(tc, out, root, before, [](const QJsonArray& a) { return QJsonValue{a}; }, "QJsonArray");
    }
}

static QList<RFC6901WriteTestCase> g_allWriteCases = collectAllWriteCases();

// A missing/unparseable data file must be a red build, not a silently empty
// suite (rfc6901-write-tests.json currently holds 50 cases).
TEST(RFC6901WriteSuiteIntegrity, CasesWereCollected) { EXPECT_GE(g_allWriteCases.size(), 40); }

INSTANTIATE_TEST_SUITE_P(RFC6901_Write,
                         RFC6901JsonPointerWriteTest,
                         ::testing::ValuesIn(g_allWriteCases),
                         [](const ::testing::TestParamInfo<RFC6901WriteTestCase>& info)
                         { return compliance_framework::sanitizeTestName(info.param.name, info.index); });

} // anonymous namespace
