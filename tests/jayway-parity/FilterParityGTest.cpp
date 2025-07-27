// FilterParityGTest.cpp
// Parity stubs for Jayway JSONPath FilterTest.java

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include "json-query/json-path/JSONPath.hpp"
#include "framework/JSONMatchersGTest.hpp"

using json_query::JSONPath;
using namespace ::testing;

namespace jayway_parity
{

// Mirror of int_eq_evals: select root objects where int-key == 1

TEST(JaywayFilterParity, IntEqFilterEvaluates)
{
    const char* json = R"({
        \"int-key\" : 1,
        \"other\"   : 2
    })";

    auto path = JSONPath::create(u"$[?(@['int-key']==1)]");
    ASSERT_TRUE(path);
    QJsonArray result = evalArray(*path, parseJson(json));
    EXPECT_THAT(result, ElementsAre(JsonObjContains(kvlist(kv("int-key", 1)))));
}

// -----------------------------------------------------------------------------
// Stub macro for remaining FilterTest methods ---------------------------------

#define FILTER_PARITY_STUB(name, reason) \
    TEST(JaywayFilterParity, DISABLED_##name) { GTEST_SKIP() << reason; }

FILTER_PARITY_STUB(IntEqStringEvals, "String compare with int pending coercion rules.");
FILTER_PARITY_STUB(LongEqEvals, "Long comparison handling.");
FILTER_PARITY_STUB(DoubleEqEvals, "Double comparison handling.");
FILTER_PARITY_STUB(StringEqEvals, "String equality.");
FILTER_PARITY_STUB(BooleanEqEvals, "Boolean equality.");
FILTER_PARITY_STUB(NullEqEvals, "Null equality.");
FILTER_PARITY_STUB(ArrEqEvals, "Array equality.");
FILTER_PARITY_STUB(IntNeEvals, "Not equal int.");
FILTER_PARITY_STUB(LongNeEvals, "Not equal long.");
FILTER_PARITY_STUB(DoubleNeEvals, "Not equal double.");
FILTER_PARITY_STUB(StringNeEvals, "Not equal string.");
FILTER_PARITY_STUB(BooleanNeEvals, "Not equal boolean.");
FILTER_PARITY_STUB(NullNeEvals, "Not equal null.");
FILTER_PARITY_STUB(IntLtEvals, "Less than int.");
FILTER_PARITY_STUB(LongLtEvals, "Less than long.");
FILTER_PARITY_STUB(DoubleLtEvals, "Less than double.");
FILTER_PARITY_STUB(StringLtEvals, "Less than string.");
FILTER_PARITY_STUB(IntLteEvals, "LTE int.");
FILTER_PARITY_STUB(LongLteEvals, "LTE long.");
FILTER_PARITY_STUB(DoubleLteEvals, "LTE double.");
FILTER_PARITY_STUB(IntGtEvals, "Greater than int.");
FILTER_PARITY_STUB(LongGtEvals, "Greater than long.");
FILTER_PARITY_STUB(DoubleGtEvals, "Greater than double.");
FILTER_PARITY_STUB(StringGtEvals, "Greater than string.");
FILTER_PARITY_STUB(IntGteEvals, "GTE int.");
FILTER_PARITY_STUB(LongGteEvals, "GTE long.");
FILTER_PARITY_STUB(DoubleGteEvals, "GTE double.");
FILTER_PARITY_STUB(StringRegexEvals, "Regex on string.");
FILTER_PARITY_STUB(ListRegexEvals, "Regex on list.");
FILTER_PARITY_STUB(ObjRegexDoesntBreak, "Regex on object.");
FILTER_PARITY_STUB(JsonEvals, "JSON equality.");
FILTER_PARITY_STUB(StringInEvals, "IN operator.");
FILTER_PARITY_STUB(StringNinEvals, "NIN operator.");
FILTER_PARITY_STUB(IntAllEvals, "ALL int array.");
FILTER_PARITY_STUB(StringAllEvals, "ALL string array.");
FILTER_PARITY_STUB(NotArrayAllEvals, "ALL on non-array.");
FILTER_PARITY_STUB(ArraySizeEvals, "SIZE array.");
FILTER_PARITY_STUB(StringSizeEvals, "SIZE string.");
FILTER_PARITY_STUB(OtherSizeEvals, "SIZE other.");
FILTER_PARITY_STUB(NullSizeEvals, "SIZE null.");
FILTER_PARITY_STUB(ArraySubsetOfEvals, "SUBSETOF array.");
FILTER_PARITY_STUB(ArrayAnyOfEvals, "ANYOF array.");
FILTER_PARITY_STUB(ArrayNoneOfEvals, "NONEOF array.");
FILTER_PARITY_STUB(ExistsEvals, "EXISTS operator.");
FILTER_PARITY_STUB(TypeEvals, "TYPE operator.");
FILTER_PARITY_STUB(NotEmptyEvals, "NOT EMPTY.");
FILTER_PARITY_STUB(EmptyEvals, "EMPTY operator.");
FILTER_PARITY_STUB(MatchesEvals, "Predicate matches.");
FILTER_PARITY_STUB(OrAndFiltersEvaluates, "OR/AND filter combinator.");
FILTER_PARITY_STUB(TestFilterWithOrShortCircuit1, "OR short circuit test1.");
FILTER_PARITY_STUB(TestFilterWithOrShortCircuit2, "OR short circuit test2.");
FILTER_PARITY_STUB(CriteriaCanBeParsed, "Criteria parse.");
FILTER_PARITY_STUB(InlineInCriteriaEvaluates, "Inline IN criteria.");

} // namespace jayway_parity

#undef FILTER_PARITY_STUB
