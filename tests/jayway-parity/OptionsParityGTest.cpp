// OptionsParityGTest.cpp
// Parity representation for Jayway OptionsTest.java

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include "json-query/json-path/JSONPath.hpp"
#include "framework/JSONMatchersGTest.hpp"

namespace jayway_parity
{

using namespace ::testing;
using json_query::JSONPath;

// -----------------------------------------------------------------------------
// Implemented simple parity test: default VALUE evaluation
// Mirrors OptionsTest.a_path_evaluation_is_returned_as_VALUE_by_default()
// -----------------------------------------------------------------------------

TEST(JaywayOptionsParity, PathEvaluationReturnedAsValueByDefault)
{
    const char* json = R"({"foo" : "bar"})";
    QJsonValue  res  = eval(u"$.foo", parseJson(json));
    EXPECT_THAT(res, IsJsonString("bar"));
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

} // namespace jayway_parity

#undef OPTIONS_PARITY_STUB
