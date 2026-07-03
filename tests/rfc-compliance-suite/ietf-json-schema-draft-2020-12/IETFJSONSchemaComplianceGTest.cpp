// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// JSONSchemaTestSuiteRunner.cpp - Parameterised Google-Test runner for JSON Schema
// Draft 2020-12 compliance test-suite.
// -----------------------------------------------------------------------------
// This file integrates the official JSON Schema Test Suite from
//   compliance/ietf-json-schema-draft-2020-12/tests/draft2020-12/*.json
// into the Google-Test infrastructure.
//
// Each JSON file contains test groups with schemas and test cases.
// The test verifies that validation results match expected outcomes.
// -----------------------------------------------------------------------------

#include "json-query/utils/BraceSafe.hpp"

#include <gtest/gtest.h>
#include <ostream>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QtDebug>

#include "json-query/json-schema/JSONSchema.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

#include "KnownFailures.hpp"

using namespace json_query::json_schema;
using json_query::literals::operator""_qt_s;

namespace
{

// ----------------------------------------------------------------------------
// File-based schema fetcher for remote $ref resolution -------------------------
// ----------------------------------------------------------------------------
static const QString kRemoteBaseUrl{u"http://localhost:1234"_qt_s};
static const QString kRemotesDir{
    QStringLiteral(JSON_QUERY_SOURCE_DIR "/compliance/ietf-json-schema-draft-2020-12/remotes")};

SchemaFetcher makeFileBasedFetcher()
{
    return [](const QString& uri) -> std::optional<QJsonValue>
    {
        // Map http://localhost:1234/path to remotes/path
        QString path{uri};
        if (path.startsWith(kRemoteBaseUrl))
            path = path.mid(kRemoteBaseUrl.size());
        if (path.startsWith(u'/'))
            path = path.mid(1);

        const auto filePath{kRemotesDir + u"/"_qt_s + path};
        QFile      file{filePath};
        if (!file.open(QIODevice::ReadOnly))
            return std::nullopt;

        QJsonParseError err{};
        const auto      doc{QJsonDocument::fromJson(file.readAll(), &err)};
        if (err.error != QJsonParseError::NoError)
            return std::nullopt;

        if (doc.isObject())
            return QJsonValue(doc.object());
        if (doc.isArray())
            return QJsonValue(doc.array());
        return std::nullopt;
    };
}

// ----------------------------------------------------------------------------
// Test-case representation ----------------------------------------------------
// ----------------------------------------------------------------------------
struct SchemaTestCase
{
    QString    fileName;              // source file name
    QString    groupDesc;             // test group description
    QString    testDesc;              // individual test description
    QJsonValue schema;                // the schema to compile
    QJsonValue data;                  // instance data to validate
    bool       expectedValid;         // expected validation result
    bool       formatAssertion{true}; // whether format validation is assertion (optional/format/) or auto
};

// Custom GoogleTest printer for readable test output
inline void PrintTo(const SchemaTestCase& tc, std::ostream* os)
{
    *os << "SchemaTestCase{"
        << "file='" << tc.fileName.toStdString() << "'"
        << ", group='" << tc.groupDesc.toStdString() << "'"
        << ", test='" << tc.testDesc.toStdString() << "'"
        << ", expected=" << (tc.expectedValid ? "valid" : "invalid") << "}";
}

// ----------------------------------------------------------------------------
// JSON loader utilities -------------------------------------------------------
// ----------------------------------------------------------------------------

#ifndef JSON_QUERY_SOURCE_DIR
#define JSON_QUERY_SOURCE_DIR "."
#endif

static QList<SchemaTestCase> loadTestFile(const QString& filePath)
{
    QList<SchemaTestCase> cases{};

    QFile file{filePath};
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning("Failed to open test file: %s", qPrintable(filePath));
        return cases;
    }

    QJsonParseError parseError{};
    const auto      doc{QJsonDocument::fromJson(file.readAll(), &parseError)};
    if (parseError.error != QJsonParseError::NoError)
    {
        qWarning("JSON parse error in %s: %s", qPrintable(filePath), qPrintable(parseError.errorString()));
        return cases;
    }

    if (!doc.isArray())
    {
        qWarning("Test file is not a JSON array: %s", qPrintable(filePath));
        return cases;
    }

    // Extract just the filename for display
    const auto fileName{QFileInfo{filePath}.fileName()};

    const auto groupArray = doc.array();
    for (const QJsonValue& groupValue : groupArray)
    {
        if (!groupValue.isObject())
            continue;

        const auto groupObj{groupValue.toObject()};
        const auto groupDesc{groupObj[u"description"_qt_s].toString()};
        const auto schema{groupObj[u"schema"_qt_s]};

        const auto testsArray{json_query::asArray(groupObj[u"tests"_qt_s])};
        for (const QJsonValue& testValue : testsArray)
        {
            if (!testValue.isObject())
                continue;

            const auto testObj{testValue.toObject()};

            SchemaTestCase tc{};
            tc.fileName      = fileName;
            tc.groupDesc     = groupDesc;
            tc.testDesc      = testObj[u"description"_qt_s].toString();
            tc.schema        = schema;
            tc.data          = testObj[u"data"_qt_s];
            tc.expectedValid = testObj[u"valid"_qt_s].toBool();

            cases.append(std::move(tc));
        }
    }

    return cases;
}

static QList<SchemaTestCase> collectAllTestCases()
{
    const QString baseDir{
        QStringLiteral(JSON_QUERY_SOURCE_DIR "/compliance/ietf-json-schema-draft-2020-12/tests/draft2020-12")};

    QList<SchemaTestCase> all{};
    QDir                  dir{baseDir};

    if (!dir.exists())
    {
        qWarning("Test suite directory not found: %s", qPrintable(baseDir));
        qWarning("Expected location: compliance/ietf-json-schema-draft-2020-12/");
        qWarning("To add the submodule, run:");
        qWarning("  git submodule add https://github.com/json-schema-org/JSON-Schema-Test-Suite.git "
                 "compliance/ietf-json-schema-draft-2020-12");
        return all;
    }

    QDirIterator it{dir.path(), QStringList{} << u"*.json"_qt_s, QDir::Files, QDirIterator::Subdirectories};
    while (it.hasNext())
    {
        const auto filePath{it.next()};
        // Tests in optional/format/ need format assertion; format.json tests annotation-only behavior
        const auto needsFormatAssertion{filePath.contains(u"/optional/format/"_qt_s) ||
                                        filePath.contains(u"/optional/format-assertion"_qt_s)};
        const auto isFormatAnnotation{QFileInfo{filePath}.fileName() == u"format.json"_qt_s &&
                                      !filePath.contains(u"/optional/"_qt_s)};
        auto       cases{loadTestFile(filePath)};
        for (auto& tc : cases)
            tc.formatAssertion = needsFormatAssertion || (!isFormatAnnotation);
        all.append(cases);
    }

    return all;
}

// ----------------------------------------------------------------------------
// Known-failure (xfail) lookup ------------------------------------------------
// ----------------------------------------------------------------------------
// Buckets apply only when the resolving optional feature is absent from the
// build; the hostname bucket applies always (strict-ASCII validator).
bool isKnownFailure(const SchemaTestCase& tc)
{
    if (matchesKnownFailure(kKnownFailuresHostname, tc.fileName, tc.groupDesc, tc.testDesc))
        return true;
#ifndef JSON_QUERY_TEST_HAS_ECMA_REGEX
    if (matchesKnownFailure(kKnownFailuresNoEcmaRegex, tc.fileName, tc.groupDesc, tc.testDesc))
        return true;
#endif
#ifndef JSON_QUERY_TEST_HAS_IDN
    if (matchesKnownFailure(kKnownFailuresNoIdn, tc.fileName, tc.groupDesc, tc.testDesc))
        return true;
#endif
    return false;
}

} // anonymous namespace

// ----------------------------------------------------------------------------
// Google-Test fixture ---------------------------------------------------------
// ----------------------------------------------------------------------------
class IETFJsonSchemaTest : public ::testing::TestWithParam<SchemaTestCase>
{
};

TEST_P(IETFJsonSchemaTest, ValidatesPerSpec)
{
    const SchemaTestCase& tc{GetParam()};

    // Compile schema with file-based fetcher for remote $ref resolution
    static const auto   fetcher{makeFileBasedFetcher()};
    const SchemaOptions opts{tc.formatAssertion ? FormatValidation::Assertion : FormatValidation::Auto};
    auto                schemaResult{JSONSchema::create(tc.schema, fetcher, opts)};
    if (!schemaResult)
    {
        if (isKnownFailure(tc))
        {
            GTEST_SKIP() << "Known failure (optional-feature gap, schema does not compile — see "
                            "KnownFailures.hpp): "
                         << tc.fileName.toStdString() << " / " << tc.groupDesc.toStdString() << " / "
                         << tc.testDesc.toStdString();
        }
        FAIL() << "Schema compilation failed for: " << tc.groupDesc.toStdString()
               << "\nError: " << to_std_sv(schemaResult.error()).data();
        return;
    }

    // Validate instance
    const auto result{schemaResult->validate(tc.data)};
    const auto isValid{result.isValid()};

    if (isValid != tc.expectedValid)
    {
        const bool debug{qEnvironmentVariableIsSet("JSON_QUERY_SCHEMA_DEBUG")};
        if (debug)
        {
            qDebug() << "=== TEST MISMATCH DEBUG ===";
            qDebug() << "File:" << tc.fileName;
            qDebug() << "Group:" << tc.groupDesc;
            qDebug() << "Test:" << tc.testDesc;
            qDebug() << "Schema:" << QJsonDocument{tc.schema.toObject()}.toJson(QJsonDocument::Compact);
            qDebug() << "Data:" << QJsonDocument::fromVariant(tc.data.toVariant()).toJson(QJsonDocument::Compact);
            qDebug() << "Expected:" << (tc.expectedValid ? "valid" : "invalid");
            qDebug() << "Got:" << (isValid ? "valid" : "invalid");
            if (!isValid)
            {
                qDebug() << "Validation errors:";
                for (const auto& error : result.errors())
                    qDebug() << "  -" << error.instanceLocation << ":" << error.message;
            }
            qDebug() << "===========================";
        }
    }

    const bool knownFailure{isKnownFailure(tc)};

    if (isValid != tc.expectedValid && knownFailure)
    {
        GTEST_SKIP() << "Known failure (optional-feature gap, see KnownFailures.hpp): "
                     << tc.fileName.toStdString() << " / " << tc.groupDesc.toStdString() << " / "
                     << tc.testDesc.toStdString();
    }

    ASSERT_FALSE(isValid == tc.expectedValid && knownFailure)
        << "Stale KnownFailures.hpp entry — this test now PASSES; remove it: " << tc.fileName.toStdString() << " / "
        << tc.groupDesc.toStdString() << " / " << tc.testDesc.toStdString();

    EXPECT_EQ(isValid, tc.expectedValid)
        << "Test: " << tc.testDesc.toStdString() << "\nGroup: " << tc.groupDesc.toStdString()
        << "\nExpected: " << (tc.expectedValid ? "valid" : "invalid") << "\nGot: " << (isValid ? "valid" : "invalid");
}

// Parameter instantiation -----------------------------------------------------
static QList<SchemaTestCase> g_allCases{collectAllTestCases()};

INSTANTIATE_TEST_SUITE_P(IETF_JSONSchema,
                         IETFJsonSchemaTest,
                         ::testing::ValuesIn(g_allCases),
                         [](const ::testing::TestParamInfo<SchemaTestCase>& info)
                         {
                             // Build unique test name: file_group_test_index
                             QString name{info.param.fileName};
                             name = name.section(u'.', 0, 0); // Remove .json extension
                             name += u"_"_qt_s + info.param.groupDesc.left(30);
                             name += u"_"_qt_s + QString::number(info.index);

                             // Replace forbidden characters with underscore
                             for (auto& ch : name)
                                 if (!ch.isLetterOrNumber())
                                     ch = QChar('_');

                             return name.toStdString();
                         });
