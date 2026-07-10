// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// JSONValueUtilsGTest.cpp - Unit tests for the as<T> conversion helpers,
// focused on the qint64 target (Qt's native JSON integer width) and the
// unified Convert-error diagnostics (expected/actual kinds in Error::detail).
#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <limits>
#include "json-query/JSONQuery"

using json_query::as;
using json_query::ConvertError;
using json_query::Error;
using json_query::ErrorDomain;
using json_query::JSONPointer;

namespace
{

void expectConvError(const Error& err, ConvertError code)
{
    EXPECT_EQ(err.domain, ErrorDomain::Convert);
    EXPECT_EQ(err.code, static_cast<std::uint8_t>(code));
}

// ---------------------------------------------------------------------------
// as<qint64>
// ---------------------------------------------------------------------------

TEST(AsQint64, ExactForIntegerBackedValuesBeyondDoublePrecision)
{
    // 2^53 + 1 is not representable as a double; Qt stores integers as int64
    // and toInteger converts losslessly
    constexpr qint64 big{(qint64{1} << 53) + 1};
    EXPECT_EQ(as<qint64>(QJsonValue{big}).value(), big);

    constexpr auto max{std::numeric_limits<qint64>::max()};
    constexpr auto min{std::numeric_limits<qint64>::min()};
    EXPECT_EQ(as<qint64>(QJsonValue{max}).value(), max);
    EXPECT_EQ(as<qint64>(QJsonValue{min}).value(), min);
}

TEST(AsQint64, IntegralDoublesConvert)
{
    EXPECT_EQ(as<qint64>(QJsonValue{42.0}).value(), qint64{42});
    EXPECT_EQ(as<qint64>(QJsonValue{-1.0}).value(), qint64{-1});
    EXPECT_EQ(as<qint64>(QJsonValue{0}).value(), qint64{0});
}

TEST(AsQint64, FractionalIsNotIntegral)
{
    auto r{as<qint64>(QJsonValue{1.5})};
    ASSERT_FALSE(r.has_value());
    expectConvError(r.error(), ConvertError::NumericNotIntegral);
}

TEST(AsQint64, HugeDoubleIsOutOfRange)
{
    auto r{as<qint64>(QJsonValue{1e300})};
    ASSERT_FALSE(r.has_value());
    expectConvError(r.error(), ConvertError::NumericOutOfRange);
}

TEST(AsQint64, NonNumberIsTypeMismatchWithKindDiagnostics)
{
    auto r{as<qint64>(QJsonValue{QStringLiteral("7")})};
    ASSERT_FALSE(r.has_value());
    expectConvError(r.error(), ConvertError::TypeMismatch);
    // detail packs expected/actual kinds; formatted_message renders them
    EXPECT_NE(r.error().detail, 0);
    const auto msg{r.error().formatted_message()};
    EXPECT_TRUE(msg.contains(QStringLiteral("expected number"))) << msg.toStdString();
    EXPECT_TRUE(msg.contains(QStringLiteral("got string"))) << msg.toStdString();
}

TEST(AsQint64, ChainsWithEvaluate)
{
    // The monadic idiom from the AC-3033 spike, now without the int detour
    constexpr qint64 id{(qint64{1} << 53) + 7};
    const QJsonDocument doc(QJsonObject{{"data", QJsonObject{{"id", id}}}});
    const auto          result{JSONPointer::create(u"/data/id")
                          .and_then([&](const JSONPointer& p) { return p.evaluate(doc); })
                          .and_then(as<qint64>)};
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, id);
}

// ---------------------------------------------------------------------------
// Unified Convert diagnostics for the existing targets
// ---------------------------------------------------------------------------

TEST(AsConversions, TypeMismatchCarriesExpectedAndActualKind)
{
    auto r{as<QJsonArray>(QJsonValue{QJsonObject{}})};
    ASSERT_FALSE(r.has_value());
    expectConvError(r.error(), ConvertError::TypeMismatch);
    const auto msg{r.error().formatted_message()};
    EXPECT_TRUE(msg.contains(QStringLiteral("expected array"))) << msg.toStdString();
    EXPECT_TRUE(msg.contains(QStringLiteral("got object"))) << msg.toStdString();
}

TEST(AsConversions, IntStillGuardsRangeAndIntegrality)
{
    EXPECT_EQ(as<int>(QJsonValue{41}).value(), 41);
    expectConvError(as<int>(QJsonValue{2.5}).error(), ConvertError::NumericNotIntegral);
    expectConvError(as<int>(QJsonValue{qint64{1} << 40}).error(), ConvertError::NumericOutOfRange);
}

} // anonymous namespace
