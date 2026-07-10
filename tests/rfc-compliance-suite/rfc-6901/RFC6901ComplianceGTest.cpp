// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// RFC6901ComplianceGTest.cpp - Parameterised Google-Test runner for JSON Pointer RFC 6901
// compliance test-suite (rfc6901-tests.json file).
// -----------------------------------------------------------------------------
// This file implements a comprehensive compliance test suite for RFC 6901 JSON Pointer
// specification, following the same pattern as the RFC 9535 JSONPath compliance suite.
//
// The test verifies:
//   1. Invalid pointer syntax ⇒ JSONPointer::create() fails with an error.
//   2. Valid pointers evaluate to expected results.
//   3. Non-existent paths return appropriate errors.
//   4. Escape sequences (~0, ~1) are handled correctly.
//   5. Special characters and edge cases work per RFC 6901.
// -----------------------------------------------------------------------------
#include <gtest/gtest.h>
#include <ostream>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QtGlobal>
#include <QtDebug>
#include <QVector>

#include "json-query/JSONQuery"

using json_query::json_pointer::JSONPointer;

namespace
{

// ----------------------------------------------------------------------------
// Test-case representation ----------------------------------------------------
// ----------------------------------------------------------------------------
struct RFC6901TestCase
{
    QString    name;                  // unique id / description
    QString    pointer;               // JSON Pointer expression
    QJsonValue document;              // input document (object or array)
    QJsonValue expectedResult;        // expected result value
    bool       invalidPointer{false}; // true if pointer should be invalid
    bool       shouldFail{false};     // true if evaluation should fail (non-existent path)
};

// Custom GoogleTest printer to avoid raw-bytes dumping for parameter values.
inline void PrintTo(const RFC6901TestCase& tc, std::ostream* os)
{
    *os << "RFC6901TestCase{"
        << "name='" << tc.name.toStdString() << "'"
        << ", pointer='" << tc.pointer.toStdString() << "'"
        << ", invalidPointer=" << (tc.invalidPointer ? "true" : "false")
        << ", shouldFail=" << (tc.shouldFail ? "true" : "false")
        << ", documentType=" << (tc.document.isArray() ? "array" : (tc.document.isObject() ? "object" : "other"))
        << "}";
}

// ----------------------------------------------------------------------------
// JSON loader utilities -------------------------------------------------------
// ----------------------------------------------------------------------------
static QList<RFC6901TestCase> loadRFC6901TestFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning("Failed to open RFC 6901 test file %s", qPrintable(path));
        return {};
    }

    const QByteArray data = f.readAll();
    QJsonParseError  perr;
    auto             doc{QJsonDocument::fromJson(data, &perr)};
    if (perr.error != QJsonParseError::NoError)
        qWarning("JSON parse error in RFC 6901 test file %s: %s", qPrintable(path), qPrintable(perr.errorString()));
    if (doc.isNull())
    {
        qWarning("Invalid JSON in RFC 6901 test file %s", qPrintable(path));
        return {};
    }

    const QJsonObject root  = doc.object();
    const QJsonArray  tests = root.value("tests").toArray();

    QList<RFC6901TestCase> out;
    for (const QJsonValue& vCase : tests)
    {
        const QJsonObject tcObj = vCase.toObject();
        RFC6901TestCase   tc;
        tc.name           = tcObj.value("name").toString();
        tc.pointer        = tcObj.value("pointer").toString();
        tc.invalidPointer = tcObj.value("invalid_pointer").toBool(false);
        tc.shouldFail     = tcObj.value("should_fail").toBool(false);

        if (!tc.invalidPointer)
        {
            tc.document = tcObj.value("document");
            if (!tc.shouldFail)
                tc.expectedResult = tcObj.value("expected");
        }
        out.append(std::move(tc));
    }
    return out;
}

#ifndef JSON_QUERY_SOURCE_DIR
#define JSON_QUERY_SOURCE_DIR "."
#endif

static QList<RFC6901TestCase> collectAllRFC6901Cases()
{
    const QString testFile =
        QStringLiteral(JSON_QUERY_SOURCE_DIR "/tests/rfc-compliance-suite/rfc-6901/rfc6901-tests.json");
    return loadRFC6901TestFile(testFile);
}

// ----------------------------------------------------------------------------
// Google-Test fixture ---------------------------------------------------------
// ----------------------------------------------------------------------------
class RFC6901JsonPointerTest : public ::testing::TestWithParam<RFC6901TestCase>
{
};

TEST_P(RFC6901JsonPointerTest, EvaluatesPerSpec)
{
    const RFC6901TestCase& tc = GetParam();

    if (tc.invalidPointer)
    {
        auto pointer{JSONPointer::create(tc.pointer)};
        EXPECT_FALSE(pointer.has_value()) << "Pointer should be invalid: " << tc.pointer.toStdString();
        return;
    }

    // Convert document to QJsonDocument (must be array/object)
    QJsonDocument doc;
    if (tc.document.isArray())
        doc = QJsonDocument(tc.document.toArray());
    else if (tc.document.isObject())
        doc = QJsonDocument(tc.document.toObject());
    else
        FAIL() << "RFC 6901 document for test '" << tc.name.toStdString() << "' is not object/array";

    // Compile pointer
    auto maybePointer{JSONPointer::create(tc.pointer)};
    ASSERT_TRUE(maybePointer.has_value()) << "Failed to compile pointer: " << tc.pointer.toStdString();

    const JSONPointer& pointer = *maybePointer;
    auto               result{pointer.evaluate(doc)};

    if (tc.shouldFail)
    {
        EXPECT_FALSE(result.has_value())
            << "Pointer evaluation should have failed for test '" << tc.name.toStdString() << "'";
        return;
    }

    if (!result.has_value())
        std::cout << "Evaluation error for " << tc.pointer.toStdString() << ": evaluation failed" << std::endl;
    ASSERT_TRUE(result.has_value()) << "Failed to evaluate pointer: " << tc.pointer.toStdString();

    QJsonValue actual = *result;

    // Compare result with expected value
    bool matched = (actual == tc.expectedResult);

    if (!matched)
    {
        qDebug() << "=== RFC 6901 TEST MISMATCH DEBUG ===";
        qDebug() << "Test name:" << tc.name;
        qDebug() << "Pointer:" << tc.pointer;
        qDebug() << "Document:" << QJsonDocument(tc.document.toObject()).toJson(QJsonDocument::Compact);
        qDebug() << "Actual result:" << QJsonDocument(QJsonArray{actual}).toJson(QJsonDocument::Compact);
        qDebug() << "Expected result:" << QJsonDocument(QJsonArray{tc.expectedResult}).toJson(QJsonDocument::Compact);
        qDebug() << "====================================";
    }

    EXPECT_TRUE(matched) << "Expected result did not match for test '" << tc.name.toStdString() << "'";
}

// Parameter instantiation -----------------------------------------------------
static QList<RFC6901TestCase> g_allCases = collectAllRFC6901Cases();

// A missing/unparseable data file must be a red build, not a silently empty
// suite (rfc6901-tests.json currently holds 37 cases).
TEST(RFC6901SuiteIntegrity, CasesWereCollected) { EXPECT_GE(g_allCases.size(), 30); }

INSTANTIATE_TEST_SUITE_P(RFC6901_Compliance,
                         RFC6901JsonPointerTest,
                         ::testing::ValuesIn(g_allCases),
                         [](const ::testing::TestParamInfo<RFC6901TestCase>& info)
                         {
                             QString n = info.param.name;
                             // Replace forbidden characters with underscore
                             for (auto& ch : n)
                                 if (!ch.isLetterOrNumber())
                                     ch = QChar('_');
                             // Append index to guarantee uniqueness
                             n += QStringLiteral("_") + QString::number(info.index);
                             return n.toStdString();
                         });

} // anonymous namespace
