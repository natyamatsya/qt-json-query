// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

// Relative singular queries (RFC 9535 §2.3.5.1, rel-singular-query):
//
//     rel-singular-query = "@" *( S ( name-segment / index-segment ) )
//     name-segment       = "[" name-selector "]" / "." member-name-shorthand
//     index-segment      = "[" index-selector "]"
//
// i.e. "@", "@.a", "@.a.b", "@['a']['b']", "@.path[0].role", "@[-1]". These
// are the operands a filter may compare, test for existence, or pass to a
// function. A singular query selects at most one node; when a member is
// absent, an index is out of range, or an intermediate value is not the
// container a segment needs, it selects nothing: the RFC's "Nothing", which
// resolve() reports as std::nullopt (never as an error).
//
// The filter compiler's per-shape parsers handle the top-level operand forms
// ("@.name op literal", ...) on their own fast paths; this module is the
// general operand form they fall back to.

#include <QJsonValue>
#include <QString>
#include <QStringView>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <variant>
#include <vector>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path::detail
{

/// Why a filter operand is not a relative singular query. Internal: the
/// filter compiler treats any of these as "not this operand form".
enum class SingularQueryError : std::uint8_t
{
    InvalidIndex,        // "[...]" content is not an RFC 9535 integer literal
    InvalidMemberName,   // ".name" is empty or not a member-name-shorthand
    InvalidQuotedName,   // "['...']" has an invalid escape / control char
    NotRelative,         // does not start with "@"
    NotSingular,         // wildcard, slice, union, filter or descendant segment
    UnexpectedCharacter, // trailing text that is no segment
    UnterminatedBracket, // "[" / quote without its closing counterpart
};

/// A compiled relative singular query: the "@" followed by name and index
/// segments, resolved against the current filter node.
class SingularQuery
{
  public:
    using Segment = std::variant<QString, qint64>; // member name | array index

    /// Parse the whole of `text` (surrounding blanks allowed) as a relative
    /// singular query.
    [[nodiscard]] static std::expected<SingularQuery, SingularQueryError> parse(QStringView text);

    /// The node this query selects from `node`, or std::nullopt for Nothing.
    [[nodiscard]] std::optional<QJsonValue> resolve(const QJsonValue& node) const;

    /// Number of segments after "@" ("@" alone has none).
    [[nodiscard]] std::size_t depth() const noexcept { return segments_.size(); }

  private:
    std::vector<Segment> segments_{};
};

/// RFC 9535 comparison-op.
enum class ComparisonOperator : std::uint8_t
{
    Equal,
    Greater,
    GreaterEqual,
    Less,
    LessEqual,
    NotEqual,
};

/// A comparable (RFC 9535 §2.3.5.1) other than a function call: a literal
/// or a relative singular query.
class Comparable
{
  public:
    /// Parse `text` as a JSON literal (number, 'string', "string", true,
    /// false, null) or a relative singular query; std::nullopt otherwise.
    [[nodiscard]] static std::optional<Comparable> parse(QStringView text);

    /// The literal, or the node the query selects from `node`; std::nullopt
    /// for Nothing.
    [[nodiscard]] std::optional<QJsonValue> evaluate(const QJsonValue& node) const;

  private:
    std::variant<QJsonValue, SingularQuery> operand_{};
};

/// A comparison split at its top-level operator.
struct ComparisonSplit
{
    QStringView        lhs{};
    ComparisonOperator op{};
    QStringView        rhs{};
};

/// Split `expr` at its first comparison operator (==, !=, <=, >=, <, >) that
/// is outside string literals, brackets and parentheses; the operands are
/// trimmed views into `expr`. std::nullopt when there is none.
[[nodiscard]] std::optional<ComparisonSplit> splitTopLevelComparison(QStringView expr);

/// RFC 9535 §2.3.5.2.2 comparison of two comparable values, std::nullopt
/// being Nothing: Nothing == Nothing; Nothing compares unequal to (and
/// unordered with) any value; numbers compare numerically, strings
/// lexically; only numbers and strings are ordered; arrays and objects
/// compare by deep equality.
[[nodiscard]] bool
compareComparables(const std::optional<QJsonValue>& lhs, ComparisonOperator op, const std::optional<QJsonValue>& rhs);

} // namespace json_query::json_path::detail
