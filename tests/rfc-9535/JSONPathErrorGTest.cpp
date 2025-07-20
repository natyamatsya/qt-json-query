// JSONPathErrorGTest.cpp - Tests adhering to RFC 9535 error semantics
// Ensures the library rejects invalid JSONPath constructs per spec.
// -----------------------------------------------------------------------------
#include <gtest/gtest.h>
#include "json-query/json-path/JSONPath.hpp"
#include "framework/JSONMatchersGTest.hpp"

using json_query::JSONPath;
using json_query::json_path::Error;
using namespace ::testing;

// Invalid predicate syntax should cause a compile-time error (UnsupportedFilter)
TEST(RFC9535_JSONPath, InvalidPredicateSyntax)
{
    auto path = JSONPath::create(u"$..foo[?(@.bar)]");
    ASSERT_FALSE(path);
    EXPECT_EQ(path.error(), Error::UnsupportedFilter);
}

// Out-of-bounds / non-array subscription: RFC 9535 requires an error or empty result.
// The compiler should succeed, but evaluation on non-array branches yields expected values only from valid array paths.
TEST(RFC9535_JSONPath, NonArraySubscriptionYieldsEmpty)
{
    auto path = JSONPath::create(u"$..[2][3]");
    ASSERT_TRUE(path);

    // Case 1: single branch with valid nested array → expect 3
    QJsonArray r1 = evalArray(*path, parseJson(R"({"x": [0,1,[0,1,2,3,null],null]})"));
    EXPECT_THAT(r1, ElementsAre(IsJsonInt(3)));

    // Case 2: two branches, only one contains nested array → still expect 3
    QJsonArray r2 = evalArray(*path, parseJson(R"({"x": [0,1,[0,1,2,3,null],null], "y": [0,1,2]})"));
    EXPECT_THAT(r2, ElementsAre(IsJsonInt(3)));

    // Case 3: nested array too short (index OOB) → expect empty
    QJsonArray r3 = evalArray(*path, parseJson(R"({"x": [0,1,[0,1,2],null], "y": [0,1,2]})"));
    EXPECT_TRUE(r3.isEmpty());
}
