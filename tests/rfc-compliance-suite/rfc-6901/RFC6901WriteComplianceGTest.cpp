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
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning("Failed to open RFC 6901 write test file %s", qPrintable(path));
        return {};
    }

    QJsonParseError perr;
    const auto      doc{QJsonDocument::fromJson(f.readAll(), &perr)};
    if (perr.error != QJsonParseError::NoError || doc.isNull())
    {
        qWarning("Invalid JSON in RFC 6901 write test file %s: %s", qPrintable(path), qPrintable(perr.errorString()));
        return {};
    }

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
        const QJsonDocument before{doc}; // COW copy for the guarantees below

        bool       ok{false};
        QJsonValue removed{QJsonValue::Undefined};
        if (tc.op == QLatin1String("add"))
            ok = pointer.add(doc, tc.value).has_value();
        else if (tc.op == QLatin1String("replace"))
            ok = pointer.replace(doc, tc.value).has_value();
        else if (tc.op == QLatin1String("remove"))
        {
            auto r{pointer.remove(doc)};
            ok = r.has_value();
            if (ok)
                removed = *r;
        }
        else if (tc.op == QLatin1String("set"))
            ok = pointer.set(doc, tc.value, options).has_value();
        else
            FAIL() << "Unknown op '" << tc.op.toStdString() << "' in test " << tc.name.toStdString();

        if (tc.shouldFail)
        {
            EXPECT_FALSE(ok) << "Write should have failed: " << tc.name.toStdString();
            EXPECT_EQ(doc, before) << "Failed write must leave the document untouched (strong guarantee): "
                                   << tc.name.toStdString();
        }
        else
        {
            ASSERT_TRUE(ok) << "Write failed unexpectedly: " << tc.name.toStdString();
            const QJsonDocument expected{toDocument(tc.expectedDocument)};
            EXPECT_EQ(doc, expected) << "Document mismatch for " << tc.name.toStdString() << "\n  actual:   "
                                     << doc.toJson(QJsonDocument::Compact).toStdString()
                                     << "\n  expected: " << expected.toJson(QJsonDocument::Compact).toStdString();
            EXPECT_EQ(before, toDocument(tc.document))
                << "Pre-write copy must be unaffected by the write (COW isolation): " << tc.name.toStdString();
            if (!tc.expectedRemoved.isUndefined())
                EXPECT_EQ(removed, tc.expectedRemoved)
                    << "remove() returned the wrong value: " << tc.name.toStdString();
        }
    }

    // ---- QJsonValue overload ----------------------------------------------
    {
        QJsonValue       root{tc.document};
        const QJsonValue before{root};

        bool       ok{false};
        QJsonValue removed{QJsonValue::Undefined};
        if (tc.op == QLatin1String("add"))
            ok = pointer.add(root, tc.value).has_value();
        else if (tc.op == QLatin1String("replace"))
            ok = pointer.replace(root, tc.value).has_value();
        else if (tc.op == QLatin1String("remove"))
        {
            auto r{pointer.remove(root)};
            ok = r.has_value();
            if (ok)
                removed = *r;
        }
        else if (tc.op == QLatin1String("set"))
            ok = pointer.set(root, tc.value, options).has_value();

        if (tc.shouldFail)
        {
            EXPECT_FALSE(ok);
            EXPECT_EQ(root, before);
        }
        else
        {
            ASSERT_TRUE(ok) << "Value-overload write failed unexpectedly: " << tc.name.toStdString();
            EXPECT_EQ(root, tc.expectedDocument);
            EXPECT_EQ(before, tc.document);
            if (!tc.expectedRemoved.isUndefined())
                EXPECT_EQ(removed, tc.expectedRemoved);
        }
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
                         {
                             QString n = info.param.name;
                             for (auto& ch : n)
                                 if (!ch.isLetterOrNumber())
                                     ch = QChar('_');
                             n += QStringLiteral("_") + QString::number(info.index);
                             return n.toStdString();
                         });

} // anonymous namespace
