// OptionsParityGTest.cpp
// Parity representation for Jayway OptionsTest.java

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include "json-query/JSONPath.hpp"

static QJsonDocument parse(const char* src) { return QJsonDocument::fromJson(QByteArray(src)); }

// -----------------------------------------------------------------------------
// Implemented simple parity test: default VALUE evaluation
// Mirrors OptionsTest.a_path_evaluation_is_returned_as_VALUE_by_default()
// -----------------------------------------------------------------------------

TEST(JaywayOptionsParity, PathEvaluationReturnedAsValueByDefault)
{
    const char* json = R"({"foo" : "bar"})";
    auto doc = parse(json);
    auto path = JSONPath::create(u"$.foo");
    ASSERT_TRUE(path);
    QJsonValue res = path->evaluate(doc);
    ASSERT_TRUE(res.isString());
    EXPECT_EQ(res.toString(), u"bar");
}

// -----------------------------------------------------------------------------
// Stub macro for remaining Java tests -----------------------------------------

#define OPTIONS_PARITY_STUB(name, reason) \
    TEST(JaywayOptionsParity, DISABLED_##name) { GTEST_SKIP() << reason; }

OPTIONS_PARITY_STUB(ALeafsIsNotDefaultedToNull, "Requires PathNotFoundException behaviour.");
OPTIONS_PARITY_STUB(ALeafsCanBeDefaultedToNull, "Needs DEFAULT_PATH_LEAF_TO_NULL option.");
OPTIONS_PARITY_STUB(ADefinitePathIsNotReturnedAsListByDefault, "List-return default semantics.");
OPTIONS_PARITY_STUB(ADefinitePathCanBeReturnedAsList, "ALWAYS_RETURN_LIST option.");
OPTIONS_PARITY_STUB(AnIndefinitePathCanBeReturnedAsList, "ALWAYS_RETURN_LIST option.");
OPTIONS_PARITY_STUB(APathEvaluationCanBeReturnedAsPathList, "AS_PATH_LIST option.");
OPTIONS_PARITY_STUB(MultiPropertiesAreMergedByDefault, "Multiprop merge semantics.");
OPTIONS_PARITY_STUB(WhenPropertyIsRequiredExceptionIsThrown, "REQUIRE_PROPERTIES enforcement.");
OPTIONS_PARITY_STUB(WhenPropertyIsRequiredExceptionIsThrown2, "REQUIRE_PROPERTIES enforcement.");
OPTIONS_PARITY_STUB(IssueSuppressExceptionsDoesNotBreakIndefiniteEvaluation, "SUPPRESS_EXCEPTIONS option.");
OPTIONS_PARITY_STUB(IsbnIsDefaultedWhenOptionIsProvided, "DEFAULT_PATH_LEAF_TO_NULL with wildcard.");

#undef OPTIONS_PARITY_STUB
