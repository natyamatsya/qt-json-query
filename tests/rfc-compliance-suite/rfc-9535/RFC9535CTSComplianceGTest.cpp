// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// CTSComplianceGTest.cpp - Parameterised Google-Test runner for JSONPath RFC 9535
// compliance test-suite (cts.json files).
// -----------------------------------------------------------------------------
// This file is generated automatically by Cascade to integrate the official
// Compliance Test Suite provided in
//   compliance/rfc-9535-jsonpath-test-suite/tests/*.json
// into the existing Google-Test infrastructure used by the project.
//
// Each JSON file matches the schema in cts.schema.json.  We parse all test cases
// and feed them as parameters to a single test fixture.  The test verifies:
//   1. Invalid selector ⇒ JSONPath::create() fails with an error.
//   2. Otherwise we expect evaluation results to match *one* of the possible
//      result sets.
// Order of matched values is ignored as mandated by RFC 9535.
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

using json_query::json_path::JSONPath;
using json_query::json_path::ParseError;

namespace
{

// ----------------------------------------------------------------------------
// Helpers for unordered array equality ---------------------------------------
// ----------------------------------------------------------------------------
static bool arraysEqualUnordered(const QJsonArray& a, const QJsonArray& b)
{
    if (a.size() != b.size())
        return false;

    QVector<bool> matched(b.size(), false);
    for (const auto& vA : a)
    {
        bool found = false;
        for (int i = 0; i < b.size(); ++i)
        {
            if (!matched[i] && vA == b.at(i))
            {
                matched[i] = true;
                found      = true;
                break;
            }
        }
        if (!found)
            return false;
    }
    return true;
}

// ----------------------------------------------------------------------------
// Test-case representation ----------------------------------------------------
// ----------------------------------------------------------------------------
struct CtsTestCase
{
    QString           name;       // unique id / description
    QString           selector;   // JSONPath expression
    QJsonValue        document;   // input document (object or array)
    QList<QJsonArray> resultSets; // one or many expected result arrays
    bool              invalidSelector{false};
};

// Custom GoogleTest printer to avoid raw-bytes dumping of padding, which
// triggers Valgrind "uninitialised value" reports. Keep it lightweight.
inline void PrintTo(const CtsTestCase& tc, std::ostream* os)
{
    *os << "CtsTestCase{"
        << "name='" << tc.name.toStdString() << "'"
        << ", selector='" << tc.selector.toStdString() << "'"
        << ", invalidSelector=" << (tc.invalidSelector ? "true" : "false")
        << ", documentType=" << (tc.document.isArray() ? "array" : (tc.document.isObject() ? "object" : "other"))
        << ", resultSets=" << tc.resultSets.size() << "}";
}

// ----------------------------------------------------------------------------
// JSON loader utilities -------------------------------------------------------
// ----------------------------------------------------------------------------
static QList<CtsTestCase> loadCtsFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning("Failed to open CTS file %s", qPrintable(path));
        return {};
    }

    const QByteArray data = f.readAll();
    QJsonParseError  perr;
    auto             doc{QJsonDocument::fromJson(data, &perr)};
    if (perr.error != QJsonParseError::NoError)
        qWarning("JSON parse error in CTS file %s: %s", qPrintable(path), qPrintable(perr.errorString()));
    if (doc.isNull())
    {
        qWarning("Invalid JSON in CTS file %s", qPrintable(path));
        return {};
    }

    const QJsonObject root  = doc.object();
    const QJsonArray  tests = root.value("tests").toArray();

    QList<CtsTestCase> out;
    for (const QJsonValue& vCase : tests)
    {
        const QJsonObject tcObj = vCase.toObject();
        CtsTestCase       tc;
        tc.name            = tcObj.value("name").toString();
        tc.selector        = tcObj.value("selector").toString();
        tc.invalidSelector = tcObj.value("invalid_selector").toBool(false);

        if (!tc.invalidSelector)
        {
            tc.document = tcObj.value("document");

            if (tcObj.contains("result"))
            {
                tc.resultSets.append(tcObj.value("result").toArray());
            }
            else if (tcObj.contains("results"))
            {
                const QJsonArray resArr = tcObj.value("results").toArray();
                for (const QJsonValue& v : resArr)
                    tc.resultSets.append(v.toArray());
            }
        }
        out.append(std::move(tc));
    }
    return out;
}

#ifndef JSON_QUERY_SOURCE_DIR
#define JSON_QUERY_SOURCE_DIR "."
#endif

static QList<CtsTestCase> collectAllCtsCases()
{
    const QString baseDir = QStringLiteral(JSON_QUERY_SOURCE_DIR "/compliance/rfc-9535-jsonpath-test-suite/tests");
    QList<CtsTestCase> all;
    QDir               dir(baseDir);
    const QStringList  files = dir.entryList(QStringList{QStringLiteral("*.json")}, QDir::Files);
    for (const QString& fileName : files)
    {
        auto vec{loadCtsFile(dir.filePath(fileName))};
        all.append(vec);
    }
    return all;
}

// ----------------------------------------------------------------------------
// Google-Test fixture ---------------------------------------------------------
// ----------------------------------------------------------------------------
class CtsJsonPathTest : public ::testing::TestWithParam<CtsTestCase>
{
};

TEST_P(CtsJsonPathTest, EvaluatesPerSpec)
{
    const CtsTestCase& tc = GetParam();

    if (tc.invalidSelector)
    {
        auto path{JSONPath::create(tc.selector)};
        EXPECT_FALSE(path.has_value()) << "Selector should be invalid: " << tc.selector.toStdString();
        return;
    }

    // Convert document to QJsonDocument (must be array/object)
    QJsonDocument doc;
    if (tc.document.isArray())
        doc = QJsonDocument(tc.document.toArray());
    else if (tc.document.isObject())
        doc = QJsonDocument(tc.document.toObject());
    else
        FAIL() << "CTS document for test '" << tc.name.toStdString() << "' is not object/array";

    // Compile selector
    qDebug() << "DEBUG: About to compile selector:" << tc.selector;
    auto maybePath{JSONPath::create(tc.selector)};
    qDebug() << "DEBUG: JSONPath::create result:" << maybePath.has_value();
    ASSERT_TRUE(maybePath.has_value()) << "Failed to compile: " << tc.selector.toStdString();

    const JSONPath& path = *maybePath;
    qDebug() << "DEBUG: About to evaluate with document";
    auto result{path.evaluate(doc)};
    qDebug() << "DEBUG: Evaluation result has_value:" << result.has_value();
    if (!result.has_value())
    {
        std::cout << "Evaluation error for " << tc.selector.toStdString() << ": " << to_std_sv(result.error())
                  << std::endl;
    }
    ASSERT_TRUE(result.has_value()) << "Failed to evaluate: " << tc.selector.toStdString();

    QJsonValue resVal = *result;

    // Evaluate - handle root selectors correctly
    QJsonArray actual;

    // Special case: root selector should preserve array results as single items
    // The root selector "$" returns the document itself, even if it's an array
    bool isRootSelector = (tc.selector == "$");

    if (isRootSelector)
    {
        // Root selector: preserve the result as a single item, even if it's an array
        actual = QJsonArray{resVal};
    }
    else
    {
        // Non-root selectors: expand arrays into individual results
        if (resVal.isArray())
            actual = resVal.toArray();
        else
            actual = QJsonArray{resVal};
    }

    // Compare against each allowed result set
    bool matched = false;
    for (const QJsonArray& expected : tc.resultSets)
    {
        if (arraysEqualUnordered(actual, expected))
        {
            matched = true;
            break;
        }
    }

    if (!matched)
    {
        const bool ctsDebug = qEnvironmentVariableIsSet("JSON_QUERY_CTS_DEBUG");
        if (ctsDebug)
        {
            qDebug() << "=== TEST MISMATCH DEBUG ===";
            qDebug() << "Test name:" << tc.name;
            qDebug() << "Selector:" << tc.selector;
            qDebug() << "Document:" << QJsonDocument(tc.document.toObject()).toJson(QJsonDocument::Compact);
            qDebug() << "Actual result (" << actual.size() << " items):";
            for (int i = 0; i < actual.size(); ++i)
                qDebug() << "  [" << i << "]" << QJsonDocument(QJsonArray{actual[i]}).toJson(QJsonDocument::Compact);
            qDebug() << "Expected result sets:";
            for (int setIdx = 0; setIdx < tc.resultSets.size(); ++setIdx)
            {
                const QJsonArray& expected = tc.resultSets[setIdx];
                qDebug() << "  Set" << setIdx << "(" << expected.size() << " items):";
                for (int i = 0; i < expected.size(); ++i)
                {
                    qDebug() << "    [" << i << "]"
                             << QJsonDocument(QJsonArray{expected[i]}).toJson(QJsonDocument::Compact);
                }
            }
            qDebug() << "=========================";
        }
    }

    EXPECT_TRUE(matched) << "No expected result set matched for test '" << tc.name.toStdString() << "'";
}

// Parameter instantiation -----------------------------------------------------
static QList<CtsTestCase> g_allCases = collectAllCtsCases();

INSTANTIATE_TEST_SUITE_P(RFC9535_CTS,
                         CtsJsonPathTest,
                         ::testing::ValuesIn(g_allCases),
                         [](const ::testing::TestParamInfo<CtsTestCase>& info)
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
