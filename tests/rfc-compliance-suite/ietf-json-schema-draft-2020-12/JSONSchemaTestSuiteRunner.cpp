// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <gtest/gtest.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "json-query/json-schema/JSONSchema.hpp"

using namespace json_query::json_schema;

namespace
{

struct TestCase
{
    QString     description;
    QJsonValue  data;
    bool        valid;
    QString     comment;
};

struct TestGroup
{
    QString                description;
    QJsonObject            schema;
    std::vector<TestCase>  tests;
    QString                comment;
};

std::vector<TestGroup> loadTestFile(const QString& filePath)
{
    std::vector<TestGroup> groups{};

    QFile file{filePath};
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "Failed to open test file:" << filePath;
        return groups;
    }

    const auto doc{QJsonDocument::fromJson(file.readAll())};
    if (!doc.isArray())
    {
        qWarning() << "Test file is not a JSON array:" << filePath;
        return groups;
    }

    const auto groupArray{doc.array()};
    for (const QJsonValue& groupValue : groupArray)
    {
        if (!groupValue.isObject())
            continue;

        const auto groupObj{groupValue.toObject()};
        TestGroup  group{};

        group.description = groupObj[u"description"_qs].toString();
        group.schema      = groupObj[u"schema"_qs].toObject();
        group.comment     = groupObj[u"comment"_qs].toString();

        const auto testsArray{groupObj[u"tests"_qs].toArray()};
        for (const QJsonValue& testValue : testsArray)
        {
            if (!testValue.isObject())
                continue;

            const auto testObj{testValue.toObject()};
            TestCase   test{};

            test.description = testObj[u"description"_qs].toString();
            test.data        = testObj[u"data"_qs];
            test.valid       = testObj[u"valid"_qs].toBool();
            test.comment     = testObj[u"comment"_qs].toString();

            group.tests.push_back(test);
        }

        groups.push_back(group);
    }

    return groups;
}

std::vector<QString> findTestFiles(const QString& testSuiteDir)
{
    std::vector<QString> files{};

    const QDir dir{testSuiteDir + u"/tests/draft2020-12"_qs};
    if (!dir.exists())
    {
        qWarning() << "Test suite directory not found:" << dir.path();
        qWarning() << "Expected location: compliance/json-schema-test-suite/";
        qWarning() << "To add the submodule, run:";
        qWarning() << "  git submodule add https://github.com/json-schema-org/JSON-Schema-Test-Suite.git compliance/json-schema-test-suite";
        return files;
    }

    QDirIterator it{dir.path(), QStringList{} << u"*.json"_qs, QDir::Files, QDirIterator::Subdirectories};
    while (it.hasNext())
    {
        files.push_back(it.next());
    }

    return files;
}

} // anonymous namespace

class JSONSchemaComplianceTest : public ::testing::TestWithParam<QString>
{
  protected:
    static QString getTestSuiteDir()
    {
#ifndef JSON_QUERY_SOURCE_DIR
#define JSON_QUERY_SOURCE_DIR "."
#endif
        return QStringLiteral(JSON_QUERY_SOURCE_DIR "/compliance/json-schema-test-suite");
    }
};

TEST_P(JSONSchemaComplianceTest, OfficialTestSuite)
{
    const auto testFile{GetParam()};
    const auto groups{loadTestFile(testFile)};

    ASSERT_FALSE(groups.empty()) << "No test groups loaded from: " << testFile.toStdString();

    for (const auto& group : groups)
    {
        SCOPED_TRACE("Test group: " + group.description.toStdString());

        auto schemaResult{JSONSchema::create(group.schema)};
        if (!schemaResult)
        {
            FAIL() << "Schema compilation failed: " << group.description.toStdString()
                   << "\nError: " << to_std_sv(schemaResult.error()).data();
            continue;
        }

        for (const auto& test : group.tests)
        {
            SCOPED_TRACE("Test case: " + test.description.toStdString());

            const auto result{schemaResult->validate(test.data)};
            const auto isValid{result.isValid()};

            if (isValid != test.valid)
            {
                QString errorMsg{};
                if (!isValid)
                {
                    errorMsg = u"\nValidation errors:\n"_qs;
                    for (const auto& error : result.errors())
                    {
                        errorMsg += u"  - "_qs + error.instanceLocation + u": "_qs + error.message + u"\n"_qs;
                    }
                }

                EXPECT_EQ(isValid, test.valid)
                    << "Test: " << test.description.toStdString() << "\nExpected: " << (test.valid ? "valid" : "invalid")
                    << "\nGot: " << (isValid ? "valid" : "invalid") << errorMsg.toStdString();
            }
        }
    }
}

INSTANTIATE_TEST_SUITE_P(Draft2020_12,
                         JSONSchemaComplianceTest,
                         ::testing::ValuesIn([]() {
                             const auto testSuiteDir{JSONSchemaComplianceTest::getTestSuiteDir()};
                             if (testSuiteDir.isEmpty())
                             {
                                 qWarning() << "JSON_SCHEMA_TEST_SUITE_DIR not defined";
                                 return std::vector<QString>{};
                             }
                             return findTestFiles(testSuiteDir);
                         }()),
                         [](const ::testing::TestParamInfo<QString>& info) {
                             QString name{info.param};
                             name = name.section(u'/', -1);
                             name = name.section(u'.', 0, 0);
                             name.replace(u'-', u'_');
                             name.replace(u' ', u'_');
                             return name.toStdString();
                         });
