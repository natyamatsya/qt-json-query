// PathCompilerParityGTest.cpp
// Parity scaffolding for Jayway PathCompilerTest.java
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
    using namespace json_query::json_path;
    using json_query::JSONPath;
    using namespace ::testing;

// Helper macro to create disabled parity stubs quickly
#define PATHCOMPILER_STUB(name, reason) \
    TEST(JaywayPathCompilerParity, DISABLED_##name) { GTEST_SKIP() << reason; }

// -----------------------------------------------------------------------------
// Root path can be compiled ---------------------------------------------------
TEST(JaywayPathCompilerParity, RootPathCanBeCompiled)
{
    // "$" root
    auto resDollar = compile(u"$");
    ASSERT_TRUE(resDollar.has_value()) << "Compilation of '$' failed";
    EXPECT_EQ(resDollar->compiled.tokens.size(), 1);
    EXPECT_EQ(resDollar->compiled.tokens.front().key, "$");

    // "@" root (current node)
    auto resAt = compile(u"@");
    ASSERT_TRUE(resAt.has_value()) << "Compilation of '@' failed";
    EXPECT_EQ(resAt->compiled.tokens.size(), 1);
    EXPECT_EQ(resAt->compiled.tokens.front().key, "@");
}

// -----------------------------------------------------------------------------
// Implemented basic compilation parity test -----------------------------------

TEST(JaywayPathCompilerParity, CompileAtSymbol)
{
    auto resAt = compile(u"@");
    ASSERT_TRUE(resAt.has_value()) << "Compilation of '@' failed";
    EXPECT_EQ(resAt->compiled.tokens.size(), 1);
    EXPECT_EQ(resAt->compiled.tokens.front().key, "@");
}

// -----------------------------------------------------------------------------
// Parity stubs (remaining ~30 Java tests) -------------------------------------
// The reasons reference unimplemented validation logic in the C++ compiler.
// -----------------------------------------------------------------------------
PATHCOMPILER_STUB(PathMustStartWithDollarOrAt, "$ or @ requirement not yet validated");
PATHCOMPILER_STUB(SquareBracketMayNotFollowPeriod, "Structural validator pending");
PATHCOMPILER_STUB(RootPathMustBeFollowedByPeriodOrBracket, "Validator pending");
TEST(JaywayPathCompilerParity, PathMayNotEndWithPeriod)
{
    auto res = compile(u"$.");
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error(), Error::TrailingDot);
}

TEST(JaywayPathCompilerParity, PathMayNotEndWithPeriod2)
{
    auto res = compile(u"$.prop.");
    ASSERT_FALSE(res.has_value());
    EXPECT_EQ(res.error(), Error::TrailingDot);
}

// -----------------------------------------------------------------------------
// Property token compilations --------------------------------------------------

TEST(JaywayPathCompilerParity, PropertyTokenCanBeCompiled)
{
    auto res1 = compile(u"$.prop");
    ASSERT_TRUE(res1.has_value());
    ASSERT_EQ(res1->compiled.tokens.size(), 2);
    EXPECT_EQ(res1->compiled.tokens[1].key, "prop");

    auto res2 = compile(u"$.1prop");
    ASSERT_TRUE(res2.has_value());
    EXPECT_EQ(res2->compiled.tokens[1].key, "1prop");

    auto res3 = compile(u"$.@prop");
    ASSERT_TRUE(res3.has_value());
    EXPECT_EQ(res3->compiled.tokens[1].key, "@prop");
}

TEST(JaywayPathCompilerParity, BracketNotationPropertyTokenCanBeCompiled)
{
    auto check = [](QStringView path, QStringView expect){
        auto res = compile(path);
        ASSERT_TRUE(res.has_value()) << "Compilation failed for " << path.toString().toStdString();
        ASSERT_EQ(res->compiled.tokens.size(), 2);
        EXPECT_EQ(res->compiled.tokens[1].key, expect);
    };

    check(u"$['prop']",  u"prop");
    check(u"$['1prop']", u"1prop");
    check(u"$['@prop']", u"@prop");
    check(u"$[  '@prop'  ]", u"@prop");
    check(u"$[\"prop\"]", u"prop");
}

TEST(JaywayPathCompilerParity, MultiPropertyTokenCanBeCompiled)
{
    auto res1 = compile(u"$['prop0', 'prop1']");
    ASSERT_TRUE(res1.has_value());
    ASSERT_EQ(res1->compiled.tokens.size(), 3);
    EXPECT_EQ(res1->compiled.tokens[1].key, "prop0");
    EXPECT_EQ(res1->compiled.tokens[2].key, "prop1");

    auto res2 = compile(u"$[  'prop0'  , 'prop1'  ]");
    ASSERT_TRUE(res2.has_value());
    ASSERT_EQ(res2->compiled.tokens.size(), 3);
    EXPECT_EQ(res2->compiled.tokens[1].key, "prop0");
    EXPECT_EQ(res2->compiled.tokens[2].key, "prop1");
}

PATHCOMPILER_STUB(PathMayNotEndWithScan, "Trailing scan validation pending");
PATHCOMPILER_STUB(PathMayNotEndWithScan2, "Trailing scan validation pending");
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

TEST(JaywayPathCompilerParity, PropertyChainCanBeCompiled)
{
    auto chain = compile(u"$.aaa.bbb.ccc");
    ASSERT_TRUE(chain.has_value());
    ASSERT_EQ(chain->compiled.tokens.size(), 4);
    EXPECT_EQ(chain->compiled.tokens[1].key, "aaa");
    EXPECT_EQ(chain->compiled.tokens[2].key, "bbb");
    EXPECT_EQ(chain->compiled.tokens[3].key, "ccc");
}

// -----------------------------------------------------------------------------
// Wildcard tests --------------------------------------------------------------
TEST(JaywayPathCompilerParity, WildcardCanBeCompiled)
{
    auto res1 = compile(u"$.*");
    ASSERT_TRUE(res1.has_value());
    ASSERT_EQ(res1->compiled.tokens.size(), 2);
    EXPECT_EQ(res1->compiled.tokens[1].kind, Token::Kind::Wildcard);

    auto res2 = compile(u"$[*]");
    ASSERT_TRUE(res2.has_value());
    ASSERT_EQ(res2->compiled.tokens[1].kind, Token::Kind::Wildcard);

    auto res3 = compile(u"$[ * ]");
    ASSERT_TRUE(res3.has_value());
    ASSERT_EQ(res3->compiled.tokens[1].kind, Token::Kind::Wildcard);
}

TEST(JaywayPathCompilerParity, WildcardCanFollowProperty)
{
    auto res1 = compile(u"$.prop[*]");
    ASSERT_TRUE(res1.has_value());
    ASSERT_EQ(res1->compiled.tokens.size(), 3);
    EXPECT_EQ(res1->compiled.tokens[1].key, "prop");
    EXPECT_EQ(res1->compiled.tokens[2].kind, Token::Kind::Wildcard);

    auto res2 = compile(u"$['prop'][*]");
    ASSERT_TRUE(res2.has_value());
    ASSERT_EQ(res2->compiled.tokens.size(), 3);
    EXPECT_EQ(res2->compiled.tokens[1].key, "prop");
    EXPECT_EQ(res2->compiled.tokens[2].kind, Token::Kind::Wildcard);
}

// -----------------------------------------------------------------------------
// Array index path ------------------------------------------------------------
TEST(JaywayPathCompilerParity, ArrayIndexPathCanBeCompiled)
{
    auto res1 = compile(u"$[1]");
    ASSERT_TRUE(res1.has_value());
    ASSERT_EQ(res1->compiled.tokens.size(), 2);
    EXPECT_EQ(res1->compiled.tokens[1].kind, Token::Kind::Index);
    EXPECT_EQ(res1->compiled.tokens[1].index, 1);

    auto res2 = compile(u"$[1,2,3]");
    ASSERT_TRUE(res2.has_value());
    ASSERT_EQ(res2->compiled.tokens.size(), 4);
    EXPECT_EQ(res2->compiled.tokens[1].index, 1);
    EXPECT_EQ(res2->compiled.tokens[2].index, 2);
    EXPECT_EQ(res2->compiled.tokens[3].index, 3);

    auto res3 = compile(u"$[ 1 , 2 , 3 ]");
    ASSERT_TRUE(res3.has_value());
    ASSERT_EQ(res3->compiled.tokens.size(), 4);
    EXPECT_EQ(res3->compiled.tokens[1].index, 1);
    EXPECT_EQ(res3->compiled.tokens[2].index, 2);
    EXPECT_EQ(res3->compiled.tokens[3].index, 3);
}

#undef PATHCOMPILER_STUB

} // namespace jayway_parity
