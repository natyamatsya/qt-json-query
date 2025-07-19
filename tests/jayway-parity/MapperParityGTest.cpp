// MapperParityGTest.cpp
// Parity representation for Jayway MapperTest.java

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "json-query/json-path/JSONPath.hpp"
#include "framework/JSONMatchersGTest.hpp"

namespace jayway_parity {
using namespace ::testing;
using json_query::JSONPath;

// -----------------------------------------------------------------------------
// Implemented basic numeric conversion cases that already work in Qt JSON stack
// -----------------------------------------------------------------------------

#include <QDateTime>

TEST(JaywayMapperParity, IntCanBeConvertedToDouble)
{
    constexpr const char* json = R"({"val": 1})";
    QJsonValue v = eval(u"$.val", parseJson(json));
    EXPECT_DOUBLE_EQ(v.toDouble(), 1.0);
}

TEST(JaywayMapperParity, IntCanBeConvertedToLong)
{
    constexpr const char* json = R"({"val": 1})";
    QJsonValue v = eval(u"$.val", parseJson(json));
    EXPECT_EQ(static_cast<long long>(v.toDouble()), 1LL);
}

TEST(JaywayMapperParity, StringCanBeConvertedToLong)
{
    constexpr const char* json = R"({"val": "1"})";
    QJsonValue v = eval(u"$.val", parseJson(json));
    ASSERT_TRUE(v.isString());
    EXPECT_EQ(v.toString().toLongLong(), 1LL);
}

TEST(JaywayMapperParity, IntCanBeConvertedToString)
{
    constexpr const char* json = R"({"val": 1})";
    QJsonValue v = eval(u"$.val", parseJson(json));
    EXPECT_EQ(QString::number(static_cast<long long>(v.toDouble())), QStringLiteral("1"));
}

TEST(JaywayMapperParity, DISABLED_BigDecimalCanBeConvertedToLong)
{
    constexpr const char* json = R"({"val": 1.5})";
    QJsonValue v = eval(u"$.val", parseJson(json));
    EXPECT_EQ(static_cast<long long>(v.toDouble()), 1LL); // truncation
}

TEST(JaywayMapperParity, LongCanBeConvertedToDate)
{
    qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    QString json = QString::fromUtf8("{\"val\": %1}").arg(nowMs);
    QJsonValue v = eval(u"$.val", parseJson(json.toUtf8().constData()));
    QDateTime dt = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(v.toDouble()));
    EXPECT_EQ(dt, QDateTime::fromMSecsSinceEpoch(nowMs));
}

TEST(JaywayMapperParity, StringCanBeConvertedToBigInteger)
{
    constexpr const char* json = R"({"val": "1"})";
    QJsonValue v = eval(u"$.val", parseJson(json));
    EXPECT_EQ(v.toString().toLongLong(), 1LL);
}

TEST(JaywayMapperParity, DISABLED_StringCanBeConvertedToBigDecimal)
{
    constexpr const char* json = R"({"val": "1.5"})";
    QJsonValue v = eval(u"$.val", parseJson(json));
    EXPECT_DOUBLE_EQ(v.toString().toDouble(), 1.5);
}

TEST(JaywayMapperParity, BooleanCanBeConvertedToPrimitive)
{
    constexpr const char* jsonTrue = R"({"val": true})";
    QJsonValue vTrue = eval(u"$.val", parseJson(jsonTrue));
    EXPECT_TRUE(vTrue.toBool());

    constexpr const char* jsonFalse = R"({"val": false})";
    QJsonValue vFalse = eval(u"$.val", parseJson(jsonFalse));
    EXPECT_FALSE(vFalse.toBool());
}

} // namespace jayway_parity
