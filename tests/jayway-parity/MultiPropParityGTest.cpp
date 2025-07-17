// MultiPropParityGTest.cpp
// Ported from Jayway JSONPath test: com/jayway/jsonpath/MultiPropTest.java
// Source repository: tests/references/jayway-json-path/json-path/src/test/java/com/jayway/jsonpath/MultiPropTest.java

#include <gtest/gtest.h>
#include <QJsonDocument>
#include <QJsonObject>
#include "json-query/JSONPath.hpp"
#include "JaywayParityGTestHelpers.hpp"
#include <gmock/gmock.h>

namespace jayway_parity
{

using namespace ::testing;
using namespace jp;

// -----------------------------------------------------------------------------
// Implemented test – expected to pass with current library --------------------

TEST(JaywayMultiPropParity, MultiPropCanBeReadFromRoot)
{
    const char* json = R"({
        "a": "a-val",
        "b": "b-val",
        "c": "c-val"
    })";
    QJsonValue v = eval(u"$['a','b']", parseJson(json));
    EXPECT_THAT(v, JsonObjContains(kvlist(kv("a", "a-val"), kv("b", "b-val"))));

    // Absent props skipped by default
    v = eval(u"$['a','d']", parseJson(json));
    EXPECT_THAT(v, JsonObjContains(kvlist(kv("a", "a-val"))));
}

// -----------------------------------------------------------------------------
// Stubs for remaining MultiPropTest methods -----------------------------------
// Use DISABLED_ + GTEST_SKIP for parity tracking until feature support exists.

#define MP_PARITY_STUB(name, reason) \
    TEST(JaywayMultiPropParity, DISABLED_##name) { GTEST_SKIP() << reason; }

MP_PARITY_STUB(MultiPropsCanBeDefaultedToNull, "Requires DEFAULT_PATH_LEAF_TO_NULL option.");
MP_PARITY_STUB(MultiPropsCanBeRequired, "Requires Option::REQUIRE_PROPERTIES enforcement.");
MP_PARITY_STUB(MultiPropsCanBeNonLeafs, "Needs support for multi-prop non-leaf selection.");
MP_PARITY_STUB(NonexistentNonLeafMultiPropsIgnored, "Depends on graceful skip semantics.");
MP_PARITY_STUB(MultiPropsWithPostFilter, "Requires predicate filter support with multi-props.");
MP_PARITY_STUB(DeepScanDoesNotAffectNonLeafMultiProps, "Combination of deep scan and multiprops.");
MP_PARITY_STUB(MultiPropsCanBeInTheMiddle, "Multiprop in middle of path segments.");
MP_PARITY_STUB(NonLeafMultiPropsCanBeRequired, "Option::REQUIRE_PROPERTIES with non-leaf multiprops.");

} // namespace jayway_parity

#undef MP_PARITY_STUB
