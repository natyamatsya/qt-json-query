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

using namespace json_query::json_schema;
using json_query::literals::operator""_qt_s;

namespace
{

// ----------------------------------------------------------------------------
// Test-case representation ----------------------------------------------------
// ----------------------------------------------------------------------------
struct SchemaTestCase
{
    QString    fileName;      // source file name
    QString    groupDesc;     // test group description
    QString    testDesc;      // individual test description
    QJsonValue schema;        // the schema to compile
    QJsonValue data;          // instance data to validate
    bool       expectedValid; // expected validation result
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

    const auto groupArray{doc.array()};
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
        auto       cases{loadTestFile(filePath)};
        all.append(cases);
    }

    return all;
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

    // Compile schema
    auto schemaResult{JSONSchema::create(tc.schema)};
    if (!schemaResult)
    {
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
