// PathCompilerParityGTest.cpp
// Parity scaffolding for Jayway PathCompilerTest.java
#include <gtest/gtest.h>
#include "json-query/JSONPath.hpp"
#include "JaywayParityGTestHelpers.hpp"
#include <gmock/gmock.h>

namespace jayway_parity 
{
    using namespace json_query;
    using namespace json_query::json_path;
using namespace ::testing;
using namespace jp;

// Helper macro to create disabled parity stubs quickly
#define PATHCOMPILER_STUB(name, reason) \
    TEST(JaywayPathCompilerParity, DISABLED_##name) { GTEST_SKIP() << reason; }

// -----------------------------------------------------------------------------
// Root path can be compiled ---------------------------------------------------
TEST(JaywayPathCompilerParity, RootPathCanBeCompiled)
{
    // "$" root
    auto resDollar = json_query::compile(u"$");
    ASSERT_TRUE(resDollar.has_value()) << "Compilation of '$' failed";
    EXPECT_EQ(resDollar->tokens.size(), 1);
    EXPECT_EQ(resDollar->tokens.front().key, "$");

    // "@" root (current node)
    auto resAt = json_query::compile(u"@");
    ASSERT_TRUE(resAt.has_value()) << "Compilation of '@' failed";
    EXPECT_EQ(resAt->tokens.size(), 1);
    EXPECT_EQ(resAt->tokens.front().key, "@");
}

// -----------------------------------------------------------------------------
// Parity stubs (remaining ~30 Java tests) -------------------------------------
// The reasons reference unimplemented validation logic in the C++ compiler.
// -----------------------------------------------------------------------------
PATHCOMPILER_STUB(PathMustStartWithDollarOrAt, "$ or @ requirement not yet validated");
PATHCOMPILER_STUB(SquareBracketMayNotFollowPeriod, "Structural validator pending");
PATHCOMPILER_STUB(RootPathMustBeFollowedByPeriodOrBracket, "Validator pending");
PATHCOMPILER_STUB(PathMayNotEndWithPeriod, "Trailing period validation pending");
PATHCOMPILER_STUB(PathMayNotEndWithPeriod2, "Trailing period validation pending");
PATHCOMPILER_STUB(PathMayNotEndWithScan, "Trailing scan validation pending");
PATHCOMPILER_STUB(PathMayNotEndWithScan2, "Trailing scan validation pending");
PATHCOMPILER_STUB(PropertyTokenCanBeCompiled, "Tokenizer parity pending");
PATHCOMPILER_STUB(BracketNotationPropertyTokenCanBeCompiled, "Tokenizer parity pending");
PATHCOMPILER_STUB(MultiPropertyTokenCanBeCompiled, "Tokenizer parity pending");
PATHCOMPILER_STUB(PropertyChainCanBeCompiled, "Tokenizer parity pending");
PATHCOMPILER_STUB(PropertyMayNotContainBlanks, "Validation pending");
PATHCOMPILER_STUB(WildcardCanBeCompiled, "Wildcard tokenizer pending");
PATHCOMPILER_STUB(WildcardCanFollowProperty, "Wildcard tokenizer pending");
PATHCOMPILER_STUB(ArrayIndexPathCanBeCompiled, "Array index tokenizer pending");
PATHCOMPILER_STUB(ArraySlicePathCanBeCompiled, "Array slice tokenizer pending");
PATHCOMPILER_STUB(InlineCriteriaCanBeParsed, "Predicate parser pending");
PATHCOMPILER_STUB(PlaceholderCriteriaCanBeParsed, "Predicate parser pending");
PATHCOMPILER_STUB(ScanTokenCanBeParsed, "Scan tokenizer pending");
PATHCOMPILER_STUB(IssuePredicateEscapedBackslashInProp, "Issue regression pending");
PATHCOMPILER_STUB(IssuePredicateBracketInRegex, "Issue regression pending");
PATHCOMPILER_STUB(IssuePredicateAndInRegex, "Issue regression pending");
PATHCOMPILER_STUB(IssuePredicateAndInProp, "Issue regression pending");
PATHCOMPILER_STUB(IssuePredicateBracketsChangePriorities, "Precedence pending");
PATHCOMPILER_STUB(IssuePredicateOrLowerPriorityThanAnd, "Precedence pending");
PATHCOMPILER_STUB(IssuePredicateCanHaveDoubleQuotes, "Quote handling pending");
PATHCOMPILER_STUB(IssuePredicateCanHaveSingleQuotes, "Quote handling pending");
PATHCOMPILER_STUB(IssuePredicateCanHaveSingleQuotesEscaped, "Quote handling pending");
PATHCOMPILER_STUB(IssuePredicateCanHaveSquareBracketInProp, "Issue regression pending");
PATHCOMPILER_STUB(FunctionCanBeCompiled, "Function parsing pending");
PATHCOMPILER_STUB(ArrayIndexesMustBeSeparatedByCommas, "Array index validation");
PATHCOMPILER_STUB(TrailingCommaAfterListNotAccepted, "Array index validation");
PATHCOMPILER_STUB(AcceptOnlySingleCommaBetweenIndexes, "Array index validation");
PATHCOMPILER_STUB(PropertyMustBeSeparatedByCommas, "Property list validation");

#undef PATHCOMPILER_STUB

} // namespace jayway_parity
