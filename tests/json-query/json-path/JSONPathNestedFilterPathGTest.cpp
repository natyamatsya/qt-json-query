// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// JSONPathNestedFilterPathGTest.cpp
// ─────────────────────────────────
// Filter operands that are nested relative singular queries (RFC 9535
// §2.3.5.1, rel-singular-query): @.a.b, @['a']['b'], @.path[0].role — in
// comparisons, existence tests and function arguments, including the
// "Nothing" semantics of a missing or non-container intermediate.

#include <gtest/gtest.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

#include "json-query/JSONQuery"
#include "json-query/json-path/JSONPathSingularQuery.hpp"

using json_query::JSONPath;
using json_query::json_path::detail::ComparisonOperator;
using json_query::json_path::detail::SingularQuery;
using json_query::json_path::detail::SingularQueryError;
using json_query::json_path::detail::splitTopLevelComparison;

using namespace Qt::StringLiterals;

// Readable gtest output for QString / QStringList (found by ADL).
static void PrintTo(const QString& s, std::ostream* os) { *os << '"' << s.toStdString() << '"'; }

namespace
{

// The shape Sourcetrail's ui_query runs filters over: an accessibility tree.
// "Status" has no rect/ref, "Graph"'s ref is not an object, "Hidden" has a
// null rect and a null object_name.
const QJsonDocument uiTree = []
{
    static const char json[] = R"JSON({"roots": [
        {"role": "window", "name": "Sourcetrail", "state": 1048576,
         "rect": {"x": 0, "y": 0, "w": 1600, "h": 1000},
         "ref": {"object_name": "main_window", "path": []},
         "children": [
            {"role": "button", "name": "Search", "state": 18,
             "rect": {"x": 1124, "y": 139, "w": 30, "h": 30},
             "ref": {"object_name": "search_button",
                     "path": [{"index": 0, "name": "", "role": "client"}]},
             "children": []},
            {"role": "button", "name": "Refresh", "state": 227,
             "rect": {"x": 10, "y": 139, "w": 120, "h": 30},
             "ref": {"object_name": "refresh_button",
                     "path": [{"index": 1, "name": "", "role": "client"},
                              {"index": 3, "name": "toolbar", "role": "tool bar"}]},
             "children": []},
            {"role": "label", "name": "Status", "state": 0, "children": []},
            {"role": "pane", "name": "Graph", "state": 0,
             "rect": {"x": 0, "y": 200, "w": 1200, "h": 800},
             "ref": "graph_view", "children": []},
            {"role": "button", "name": "Hidden", "state": 32768,
             "rect": null, "ref": {"object_name": null}, "children": []}
         ]}
    ]})JSON";
    return QJsonDocument::fromJson(QByteArray{json});
}();

// The sorted "name" members of the nodes `path` selects from `doc` (a
// "<...>" marker on a compile or evaluation failure).
[[nodiscard]] QStringList selectNames(QStringView path, const QJsonDocument& doc = uiTree)
{
    const auto compiled{JSONPath::create(path)};
    if (!compiled)
    {
        ADD_FAILURE() << "does not compile: " << path.toString().toStdString() << ": " << to_std_sv(compiled.error());
        return {u"<compile error>"_s};
    }
    const auto result{compiled->evaluate(doc)};
    if (!result)
    {
        ADD_FAILURE() << "does not evaluate: " << path.toString().toStdString();
        return {u"<evaluation error>"_s};
    }
    QStringList names{};
    for (const auto& node : *result)
        names.append(node[u"name"_s].toString());
    names.sort();
    return names;
}

} // namespace

// ----------------------------------------------------------
// Comparisons on nested members (the reported cases)
// ----------------------------------------------------------

TEST(JSONPathNestedFilterPath, NestedMemberEqualsString)
{
    EXPECT_EQ(selectNames(u"$..[?(@.ref.object_name=='search_button')]"), QStringList{u"Search"_s});
}

TEST(JSONPathNestedFilterPath, NestedMemberGreaterThanNumber)
{
    EXPECT_EQ(selectNames(u"$..[?(@.rect.w > 1000)]"), (QStringList{u"Graph"_s, u"Sourcetrail"_s}));
}

TEST(JSONPathNestedFilterPath, NestedMemberInConjunction)
{
    EXPECT_EQ(selectNames(u"$..[?(@.role=='button' && @.rect.w > 100)]"), QStringList{u"Refresh"_s});
}

TEST(JSONPathNestedFilterPath, NestedMemberInDisjunction)
{
    EXPECT_EQ(selectNames(u"$..[?(@.rect.w == 30 || @.ref.object_name == 'main_window')]"),
              (QStringList{u"Search"_s, u"Sourcetrail"_s}));
}

TEST(JSONPathNestedFilterPath, NestedMemberWithoutParentheses)
{
    EXPECT_EQ(selectNames(u"$..[?@.ref.object_name=='search_button']"), QStringList{u"Search"_s});
}

TEST(JSONPathNestedFilterPath, NumberLiteralForms)
{
    EXPECT_EQ(selectNames(u"$..[?(@.rect.w == 30.0)]"), QStringList{u"Search"_s});
    EXPECT_EQ(selectNames(u"$..[?(@.rect.w == 3e1)]"), QStringList{u"Search"_s});
    EXPECT_EQ(selectNames(u"$..[?(@.rect.x < -1)]"), QStringList{});
}

TEST(JSONPathNestedFilterPath, LiteralOnTheLeft)
{
    EXPECT_EQ(selectNames(u"$..[?(1000 < @.rect.w)]"), (QStringList{u"Graph"_s, u"Sourcetrail"_s}));
    EXPECT_EQ(selectNames(u"$..[?('search_button' == @.ref.object_name)]"), QStringList{u"Search"_s});
}

TEST(JSONPathNestedFilterPath, OperatorInsideStringLiteral)
{
    const auto doc{QJsonDocument::fromJson(R"([{"name": "a", "k": {"v": "x==y"}},
                                                {"name": "b", "k": {"v": "x<y"}}])")};
    EXPECT_EQ(selectNames(u"$[?(@.k.v == 'x==y')]", doc), QStringList{u"a"_s});
    EXPECT_EQ(selectNames(u"$[?(@.k.v == 'x<y')]", doc), QStringList{u"b"_s});
}

// ----------------------------------------------------------
// Both operands nested
// ----------------------------------------------------------

TEST(JSONPathNestedFilterPath, BothSidesNested)
{
    // Status (no rect) and Hidden (null rect) match too: Nothing == Nothing.
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.rect.w == @.rect.h)]"),
              (QStringList{u"Hidden"_s, u"Search"_s, u"Status"_s}));
    EXPECT_EQ(selectNames(u"$..[?(@.rect.w && @.rect.w == @.rect.h)]"), QStringList{u"Search"_s});
    EXPECT_EQ(selectNames(u"$..[?(@.rect.w > @.rect.h)]"), (QStringList{u"Graph"_s, u"Refresh"_s, u"Sourcetrail"_s}));
    EXPECT_EQ(selectNames(u"$..[?(@.ref.path[0].index < @.ref.path[1].index)]"), QStringList{u"Refresh"_s});
}

TEST(JSONPathNestedFilterPath, NestedAgainstTopLevelMember)
{
    EXPECT_EQ(selectNames(u"$..[?(@.rect.h < @.state)]"), (QStringList{u"Refresh"_s, u"Sourcetrail"_s}));
}

// ----------------------------------------------------------
// Bracket notation and index segments
// ----------------------------------------------------------

TEST(JSONPathNestedFilterPath, BracketNotation)
{
    EXPECT_EQ(selectNames(u"$..[?(@['ref']['object_name']=='search_button')]"), QStringList{u"Search"_s});
    EXPECT_EQ(selectNames(uR"($..[?(@["ref"]["object_name"]=="search_button")])"), QStringList{u"Search"_s});
    EXPECT_EQ(selectNames(u"$..[?(@.ref['object_name']=='search_button')]"), QStringList{u"Search"_s});
    EXPECT_EQ(selectNames(u"$..[?(@['ref'].object_name=='search_button')]"), QStringList{u"Search"_s});
}

TEST(JSONPathNestedFilterPath, BracketNotationWithEscapedAndSpecialNames)
{
    const auto doc{QJsonDocument::fromJson(R"([{"name": "a", "we'ird": {"x.y": 1}},
                                                {"name": "b", "we'ird": {"x.y": 2}}])")};
    EXPECT_EQ(selectNames(uR"($[?(@['we\'ird']['x.y'] == 2)])", doc), QStringList{u"b"_s});
    EXPECT_EQ(selectNames(uR"($[?(@["we'ird"]["x.y"] == 1)])", doc), QStringList{u"a"_s});
}

TEST(JSONPathNestedFilterPath, IndexSegments)
{
    EXPECT_EQ(selectNames(u"$..[?(@.ref.path[0].role=='client')]"), (QStringList{u"Refresh"_s, u"Search"_s}));
    EXPECT_EQ(selectNames(u"$..[?(@.ref.path[-1].name=='toolbar')]"), QStringList{u"Refresh"_s});
    EXPECT_EQ(selectNames(u"$..[?(@.ref.path[1].index == 3)]"), QStringList{u"Refresh"_s});

    // @.path[0].role selects the ref objects themselves
    const auto compiled{JSONPath::create(u"$..[?(@.path[0].role=='client')]")};
    ASSERT_TRUE(compiled.has_value());
    const auto refs{compiled->evaluate(uiTree)};
    ASSERT_TRUE(refs.has_value());
    QStringList objectNames{};
    for (const auto& ref : *refs)
        objectNames.append(ref[u"object_name"_s].toString());
    objectNames.sort();
    EXPECT_EQ(objectNames, (QStringList{u"refresh_button"_s, u"search_button"_s}));
}

// ----------------------------------------------------------
// Existence tests
// ----------------------------------------------------------

TEST(JSONPathNestedFilterPath, ExistenceTest)
{
    EXPECT_EQ(selectNames(u"$..[?(@.rect.w)]"),
              (QStringList{u"Graph"_s, u"Refresh"_s, u"Search"_s, u"Sourcetrail"_s}));
    // A null member exists; a member of a non-object does not.
    EXPECT_EQ(selectNames(u"$..[?(@.ref.object_name)]"),
              (QStringList{u"Hidden"_s, u"Refresh"_s, u"Search"_s, u"Sourcetrail"_s}));
    EXPECT_EQ(selectNames(u"$..[?(@['ref']['path'][1])]"), QStringList{u"Refresh"_s});
    EXPECT_EQ(selectNames(u"$..[?@.ref.path[0].role]"), (QStringList{u"Refresh"_s, u"Search"_s}));
}

TEST(JSONPathNestedFilterPath, NegatedExistenceTest)
{
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(!@.rect.w)]"), (QStringList{u"Hidden"_s, u"Status"_s}));
}

// ----------------------------------------------------------
// Missing / non-container intermediates: Nothing, never an error
// ----------------------------------------------------------

TEST(JSONPathNestedFilterPath, MissingIntermediateComparesAsNothing)
{
    // Status has no ref, Graph's ref is a string, Hidden's object_name is null.
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.ref.object_name == 'nope')]"), QStringList{});
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.ref.object_name != 'search_button')]"),
              (QStringList{u"Graph"_s, u"Hidden"_s, u"Refresh"_s, u"Status"_s}));
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.rect.w < 100000)]"),
              (QStringList{u"Graph"_s, u"Refresh"_s, u"Search"_s}));
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.rect.w >= 0)]"),
              (QStringList{u"Graph"_s, u"Refresh"_s, u"Search"_s}));
}

TEST(JSONPathNestedFilterPath, NothingEqualsNothing)
{
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.ref.object_name == @.no.such)]"),
              (QStringList{u"Graph"_s, u"Status"_s}));
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.ref.object_name != @.no.such)]"),
              (QStringList{u"Hidden"_s, u"Refresh"_s, u"Search"_s}));
    // RFC 9535: a <= b is a < b or a == b, so Nothing <= Nothing holds
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.rect.w <= @.no.such)]"), (QStringList{u"Hidden"_s, u"Status"_s}));
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.rect.w < @.no.such)]"), QStringList{});
}

TEST(JSONPathNestedFilterPath, WrongContainerKindIsNothing)
{
    // index segment on an object, name segment on an array, out-of-range index
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.rect[0])]"), QStringList{});
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.ref.path.index == 0)]"), QStringList{});
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.ref.path[7].index == 0)]"), QStringList{});
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.ref.path[-7])]"), QStringList{});
}

// ----------------------------------------------------------
// Function arguments
// ----------------------------------------------------------

TEST(JSONPathNestedFilterPath, LengthOfNestedMember)
{
    EXPECT_EQ(selectNames(u"$..[?(length(@.ref.path) == 2)]"), QStringList{u"Refresh"_s});
    EXPECT_EQ(selectNames(u"$..[?(length(@.ref.object_name) >= 14)]"), QStringList{u"Refresh"_s});
    EXPECT_EQ(selectNames(u"$..[?(length(@.ref.path[0].role) == 6)]"), (QStringList{u"Refresh"_s, u"Search"_s}));
}

TEST(JSONPathNestedFilterPath, FunctionOfNothingIsNothing)
{
    // length() of Nothing is Nothing, not null; ordering against it is false.
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(length(@.ref.object_name) == null)]"), QStringList{});
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(length(@.ref.object_name) < 100)]"),
              (QStringList{u"Refresh"_s, u"Search"_s}));
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(value(@.rect.w) == null)]"), QStringList{});
}

TEST(JSONPathNestedFilterPath, ValueOfNestedMember)
{
    EXPECT_EQ(selectNames(u"$..[?(value(@.rect.w) == 30)]"), QStringList{u"Search"_s});
}

TEST(JSONPathNestedFilterPath, FunctionAgainstNestedMember)
{
    // Status, Graph and Hidden: both operands are Nothing.
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(length(@.ref.path) == @.ref.path[0].index)]"),
              (QStringList{u"Graph"_s, u"Hidden"_s, u"Status"_s}));
    EXPECT_EQ(selectNames(u"$..[?(length(@.ref.path) > @.ref.path[0].index)]"),
              (QStringList{u"Refresh"_s, u"Search"_s}));
}

// The CTS whitespace/ cases "{spaces,newlines,tabs,returns} in a relative
// singular selector" (that directory is not part of the rfc9535 driver run):
// blanks are allowed between the segments of a singular query.
TEST(JSONPathNestedFilterPath, BlanksBetweenSegments)
{
    const auto doc{QJsonDocument::fromJson(R"([{"name": "x", "a": {"b": "foo"}}, {"name": "y"}])")};
    for (const auto* path : {u"$[?length(@ .a .b) == 3]",
                             u"$[?length(@\n.a\n.b) == 3]",
                             u"$[?length(@\t.a\t.b) == 3]",
                             u"$[?length(@\r.a\r.b) == 3]",
                             u"$[?@ .a ['b'] == 'foo']"})
        EXPECT_EQ(selectNames(QStringView{path}, doc), QStringList{u"x"_s})
            << QStringView{path}.toString().toStdString();
}

// ----------------------------------------------------------
// Still rejected: non-singular queries in comparisons
// ----------------------------------------------------------

TEST(JSONPathNestedFilterPath, NonSingularComparisonOperandsRejected)
{
    for (const auto* path : {u"$[?@.a.*==1]",
                             u"$[?@.a[*]==1]",
                             u"$[?@.a..b==1]",
                             u"$[?@.a[0:1]==1]",
                             u"$[?@.a['b','c']==1]",
                             u"$[?@.a[?@.b]==1]",
                             u"$[?@.a[01]==1]",
                             u"$[?@.a.==1]"})
        EXPECT_FALSE(JSONPath::create(QStringView{path}).has_value()) << QStringView{path}.toString().toStdString();
}

// ----------------------------------------------------------
// Top-level filters keep working (Sourcetrail's existing queries)
// ----------------------------------------------------------

TEST(JSONPathNestedFilterPath, TopLevelFiltersUnchanged)
{
    EXPECT_EQ(selectNames(u"$..[?(@.state > 200)]"), (QStringList{u"Hidden"_s, u"Refresh"_s, u"Sourcetrail"_s}));
    EXPECT_EQ(selectNames(u"$..[?(@.name=='Refresh' || @.name=='Search')]"), (QStringList{u"Refresh"_s, u"Search"_s}));
    EXPECT_EQ(selectNames(u"$..[?(@.state == 227)]"), QStringList{u"Refresh"_s});
    EXPECT_EQ(selectNames(u"$.roots[0].children[?(@.rect)]"),
              (QStringList{u"Graph"_s, u"Hidden"_s, u"Refresh"_s, u"Search"_s}));
}

// ----------------------------------------------------------
// The operand parser itself
// ----------------------------------------------------------

TEST(JSONPathSingularQuery, ParsesSingularQueries)
{
    for (const auto* text : {u"@",
                             u"@.a",
                             u"@.a.b",
                             u"@['a']['b']",
                             u"@[\"a\"][0]",
                             u"@.path[0].role",
                             u"@[-1]",
                             u"@.a [0] .b",
                             u"@._x9",
                             u"@.\u00e9t\u00e9"})
        EXPECT_TRUE(SingularQuery::parse(QStringView{text}).has_value()) << QStringView{text}.toString().toStdString();
    EXPECT_EQ(SingularQuery::parse(u"@.path[0].role")->depth(), 3U);
}

TEST(JSONPathSingularQuery, RejectsWithNamedErrors)
{
    const auto errorOf{[](QStringView text) { return SingularQuery::parse(text).error(); }};
    EXPECT_EQ(errorOf(u"$.a"), SingularQueryError::NotRelative);
    EXPECT_EQ(errorOf(u"@.*"), SingularQueryError::NotSingular);
    EXPECT_EQ(errorOf(u"@..a"), SingularQueryError::NotSingular);
    EXPECT_EQ(errorOf(u"@[1:2]"), SingularQueryError::NotSingular);
    EXPECT_EQ(errorOf(u"@['a','b']"), SingularQueryError::NotSingular);
    EXPECT_EQ(errorOf(u"@.1a"), SingularQueryError::InvalidMemberName);
    EXPECT_EQ(errorOf(u"@.a."), SingularQueryError::InvalidMemberName);
    EXPECT_EQ(errorOf(u"@[01]"), SingularQueryError::InvalidIndex);
    EXPECT_EQ(errorOf(u"@[ 0]"), SingularQueryError::InvalidIndex);
    EXPECT_EQ(errorOf(u"@['\\x']"), SingularQueryError::InvalidQuotedName);
    EXPECT_EQ(errorOf(u"@['a'"), SingularQueryError::UnterminatedBracket);
    EXPECT_EQ(errorOf(u"@.a == 1"), SingularQueryError::UnexpectedCharacter);
}

TEST(JSONPathSingularQuery, ResolvesToNothingInsteadOfFailing)
{
    const auto node{QJsonDocument::fromJson(R"({"a": {"b": null, "c": [10, 20]}, "s": "x"})").object()};
    EXPECT_EQ(SingularQuery::parse(u"@.a.b")->resolve(node), QJsonValue{QJsonValue::Null});
    EXPECT_EQ(SingularQuery::parse(u"@.a.c[-1]")->resolve(node), QJsonValue{20});
    EXPECT_FALSE(SingularQuery::parse(u"@.a.x")->resolve(node).has_value());
    EXPECT_FALSE(SingularQuery::parse(u"@.s.x")->resolve(node).has_value());
    EXPECT_FALSE(SingularQuery::parse(u"@.a.c[2]")->resolve(node).has_value());
    EXPECT_FALSE(SingularQuery::parse(u"@.a[0]")->resolve(node).has_value());
}

TEST(JSONPathSingularQuery, SplitsAtTopLevelOperatorOnly)
{
    const auto split{splitTopLevelComparison(u"@['a<b'].c <= '>=' ")};
    ASSERT_TRUE(split.has_value());
    EXPECT_EQ(split->lhs.toString(), u"@['a<b'].c"_s);
    EXPECT_EQ(split->op, ComparisonOperator::LessEqual);
    EXPECT_EQ(split->rhs.toString(), u"'>='"_s);
    EXPECT_FALSE(splitTopLevelComparison(u"!@.a.b").has_value());
}
