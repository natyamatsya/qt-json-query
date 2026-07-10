// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// RFC7386ComplianceGTest.cpp - Parameterised Google-Test runner for the JSON
// Merge Patch suite (rfc7386-tests.json). Cases a01-a15 are the complete
// RFC 7386 Appendix A example table.
// merge_patch is total (no error path), so every case is target + patch ->
// expected; targets, patches, and results may be any JSON type.
#include <gtest/gtest.h>
#include <ostream>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#include "json-query/JSONQuery"
#include "json-query/json-patch/JSONMergePatch.hpp"

using json_query::json_patch::merge_patch;

namespace
{

struct RFC7386TestCase
{
    QString    name;
    QJsonValue target;
    QJsonValue patch;
    QJsonValue expected;
};

inline void PrintTo(const RFC7386TestCase& tc, std::ostream* os)
{
    *os << "RFC7386TestCase{name='" << tc.name.toStdString() << "'}";
}

#ifndef JSON_QUERY_SOURCE_DIR
#define JSON_QUERY_SOURCE_DIR "."
#endif

static QList<RFC7386TestCase> collectAllMergePatchCases()
{
    const QString path{
        QStringLiteral(JSON_QUERY_SOURCE_DIR "/tests/rfc-compliance-suite/rfc-7386/rfc7386-tests.json")};
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
    {
        qWarning("Failed to open RFC 7386 test file %s", qPrintable(path));
        return {};
    }

    QJsonParseError perr;
    const auto      doc{QJsonDocument::fromJson(f.readAll(), &perr)};
    if (perr.error != QJsonParseError::NoError || doc.isNull())
    {
        qWarning("Invalid JSON in RFC 7386 test file %s: %s", qPrintable(path), qPrintable(perr.errorString()));
        return {};
    }

    QList<RFC7386TestCase> out;
    const QJsonArray       tests = doc.object().value("tests").toArray();
    for (const QJsonValue& vCase : tests)
    {
        const QJsonObject tcObj = vCase.toObject();
        out.append(RFC7386TestCase{
            tcObj.value(QLatin1String("name")).toString(),
            tcObj.value(QLatin1String("target")),
            tcObj.value(QLatin1String("patch")),
            tcObj.value(QLatin1String("expected")),
        });
    }
    return out;
}

class RFC7386MergePatchTest : public ::testing::TestWithParam<RFC7386TestCase>
{
};

TEST_P(RFC7386MergePatchTest, MergesPerSpec)
{
    const RFC7386TestCase& tc = GetParam();

    const QJsonValue before{tc.target};
    const auto       result{merge_patch(tc.target, tc.patch)};

    EXPECT_EQ(result, tc.expected) << "Merge mismatch for " << tc.name.toStdString();
    EXPECT_EQ(tc.target, before) << "merge_patch must not mutate its input";

    // Document overload agrees whenever both inputs are documents
    if ((tc.target.isObject() || tc.target.isArray()) && (tc.patch.isObject() || tc.patch.isArray()))
    {
        const auto toDoc = [](const QJsonValue& v)
        { return v.isArray() ? QJsonDocument{v.toArray()} : QJsonDocument{v.toObject()}; };
        const auto docResult{merge_patch(toDoc(tc.target), toDoc(tc.patch))};
        if (tc.expected.isObject())
            EXPECT_EQ(docResult.object(), tc.expected.toObject());
        else if (tc.expected.isArray())
            EXPECT_EQ(docResult.array(), tc.expected.toArray());
    }
}

static QList<RFC7386TestCase> g_allMergeCases = collectAllMergePatchCases();

// A missing/unparseable data file must be a red build, not a silently empty
// suite (rfc7386-tests.json currently holds 20 cases: the full Appendix A
// table plus edge cases).
TEST(RFC7386SuiteIntegrity, CasesWereCollected) { EXPECT_GE(g_allMergeCases.size(), 15); }

INSTANTIATE_TEST_SUITE_P(RFC7386_Compliance,
                         RFC7386MergePatchTest,
                         ::testing::ValuesIn(g_allMergeCases),
                         [](const ::testing::TestParamInfo<RFC7386TestCase>& info)
                         {
                             QString n = info.param.name;
                             for (auto& ch : n)
                                 if (!ch.isLetterOrNumber())
                                     ch = QChar('_');
                             n += QStringLiteral("_") + QString::number(info.index);
                             return n.toStdString();
                         });

} // anonymous namespace
