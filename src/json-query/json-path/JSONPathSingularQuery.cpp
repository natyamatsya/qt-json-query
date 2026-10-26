// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-path/JSONPathSingularQuery.hpp"
#include "json-query/json-path/JSONPathParseUtils.hpp"
#include "json-query/json-path/JSONPathFilter.hpp" // isValidJsonNumber

#include <QJsonArray>
#include <QJsonObject>

#include <algorithm>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path::detail
{

namespace
{

// RFC 9535 §2.5.1.1: name-first = ALPHA / "_" / %x80-D7FF / %xE000-10FFFF.
// Every UTF-16 code unit >= 0x80 belongs to one of those ranges or to a
// surrogate pair encoding the supplementary planes.
[[nodiscard]] constexpr bool isNameFirst(char16_t c) noexcept
{
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z') || c == u'_' || c >= 0x80;
}

[[nodiscard]] constexpr bool isNameChar(char16_t c) noexcept { return isNameFirst(c) || (c >= u'0' && c <= u'9'); }

// RFC 9535 blank: space, horizontal tab, line feed, carriage return.
[[nodiscard]] constexpr bool isBlank(char16_t c) noexcept
{
    return c == u' ' || c == u'\t' || c == u'\n' || c == u'\r';
}

// ".name": pos is on the dot; on success pos is past the name.
[[nodiscard]] std::expected<QString, SingularQueryError> scanShorthand(QStringView text, qsizetype& pos)
{
    ++pos; // '.'
    if (pos >= text.size())
        return std::unexpected(SingularQueryError::InvalidMemberName);
    const auto first{text[pos].unicode()};
    if (first == u'.' || first == u'*')
        return std::unexpected(SingularQueryError::NotSingular); // "..": descendant, ".*": wildcard
    if (!isNameFirst(first))
        return std::unexpected(SingularQueryError::InvalidMemberName);

    const auto start{pos};
    while (pos < text.size() && isNameChar(text[pos].unicode()))
        ++pos;
    return text.sliced(start, pos - start).toString();
}

// "['name']" / "[\"name\"]": pos is on the opening quote; on success pos is
// past the closing quote (the caller consumes the ']').
[[nodiscard]] std::expected<QString, SingularQueryError> scanQuotedName(QStringView text, qsizetype& pos)
{
    const auto quote{text[pos]};
    const auto start{pos + 1};
    auto       end{start};
    while (end < text.size() && text[end] != quote)
        end += (text[end] == u'\\') ? 2 : 1;
    if (end >= text.size())
        return std::unexpected(SingularQueryError::UnterminatedBracket);

    const auto content{text.sliced(start, end - start)};
    const auto style{quote == u'\'' ? QuoteStyle::Single : QuoteStyle::Double};
    if (!isValidQuotedKey(content, style))
        return std::unexpected(SingularQueryError::InvalidQuotedName);

    pos = end + 1;
    return unescapeQuotedKey(content);
}

// "[index]": pos is on the first character after '['; on success pos is on
// the closing ']'.
[[nodiscard]] std::expected<qint64, SingularQueryError> scanIndex(QStringView text, qsizetype& pos)
{
    const auto close{text.indexOf(u']', pos)};
    if (close < 0)
        return std::unexpected(SingularQueryError::UnterminatedBracket);

    const auto content{text.sliced(pos, close - pos)};
    if (content.contains(u':') || content.contains(u',') || content.contains(u'*') || content.contains(u'?'))
        return std::unexpected(SingularQueryError::NotSingular); // slice, union, wildcard, filter
    // The singular-query grammar has no blanks inside the brackets, and
    // isValidIndexLiteral would trim them.
    if (content.isEmpty() || isBlank(content.front().unicode()) || isBlank(content.back().unicode()) ||
        !isValidIndexLiteral(content))
        return std::unexpected(SingularQueryError::InvalidIndex);

    auto       ok{false};
    const auto index{content.toLongLong(&ok)};
    if (!ok)
        return std::unexpected(SingularQueryError::InvalidIndex);
    pos = close;
    return index;
}

[[nodiscard]] std::expected<SingularQuery::Segment, SingularQueryError> scanBracket(QStringView text, qsizetype& pos)
{
    ++pos; // '['
    if (pos >= text.size())
        return std::unexpected(SingularQueryError::UnterminatedBracket);

    if (text[pos] == u'\'' || text[pos] == u'"')
    {
        auto name{scanQuotedName(text, pos)};
        if (!name)
            return std::unexpected(name.error());
        if (pos >= text.size())
            return std::unexpected(SingularQueryError::UnterminatedBracket);
        if (text[pos] != u']')
            return std::unexpected(text[pos] == u',' ? SingularQueryError::NotSingular
                                                     : SingularQueryError::UnterminatedBracket);
        ++pos;
        return SingularQuery::Segment{std::move(*name)};
    }

    auto index{scanIndex(text, pos)};
    if (!index)
        return std::unexpected(index.error());
    ++pos; // ']'
    return SingularQuery::Segment{*index};
}

} // namespace

std::expected<SingularQuery, SingularQueryError> SingularQuery::parse(QStringView text)
{
    text = text.trimmed();
    if (text.isEmpty() || text.front() != u'@')
        return std::unexpected(SingularQueryError::NotRelative);

    SingularQuery query{};
    qsizetype     pos{1};
    while (pos < text.size())
    {
        // S between segments (the text is trimmed, so blanks here are
        // followed by more text).
        while (isBlank(text[pos].unicode()))
            ++pos;

        if (text[pos] == u'.')
        {
            auto name{scanShorthand(text, pos)};
            if (!name)
                return std::unexpected(name.error());
            query.segments_.emplace_back(std::move(*name));
        }
        else if (text[pos] == u'[')
        {
            auto segment{scanBracket(text, pos)};
            if (!segment)
                return std::unexpected(segment.error());
            query.segments_.push_back(std::move(*segment));
        }
        else
        {
            return std::unexpected(SingularQueryError::UnexpectedCharacter);
        }
    }
    return query;
}

std::optional<QJsonValue> SingularQuery::resolve(const QJsonValue& node) const
{
    QJsonValue current{node};
    for (const auto& segment : segments_)
    {
        if (const auto* name{std::get_if<QString>(&segment)})
        {
            if (!current.isObject())
                return std::nullopt;
            const auto obj{current.toObject()};
            const auto it{obj.constFind(*name)};
            if (it == obj.constEnd())
                return std::nullopt;
            current = it.value();
        }
        else
        {
            if (!current.isArray())
                return std::nullopt;
            const QJsonArray arr = current.toArray(); // ADR-001: copy-init
            const auto       index{std::get<qint64>(segment)};
            const auto       normalized{index < 0 ? index + arr.size() : index};
            if (normalized < 0 || normalized >= arr.size())
                return std::nullopt;
            current = arr.at(normalized);
        }
    }
    return current;
}

std::optional<Comparable> Comparable::parse(QStringView text)
{
    text = text.trimmed();
    if (text.isEmpty())
        return std::nullopt;

    Comparable comparable{};
    if (text.front() == u'@')
    {
        auto query{SingularQuery::parse(text)};
        if (!query)
            return std::nullopt;
        comparable.operand_ = std::move(*query);
        return comparable;
    }

    if (text == u"null")
        comparable.operand_ = QJsonValue{QJsonValue::Null};
    else if (text == u"true" || text == u"false")
        comparable.operand_ = QJsonValue{text == u"true"};
    else if (const auto quote{text.front()};
             (quote == u'\'' || quote == u'"') && text.size() >= 2 && text.back() == quote)
    {
        const auto content{text.sliced(1, text.size() - 2)};
        if (!isValidQuotedKey(content, quote == u'\'' ? QuoteStyle::Single : QuoteStyle::Double))
            return std::nullopt;
        comparable.operand_ = QJsonValue{unescapeQuotedKey(content)};
    }
    else if (const auto number{text.toString()}; isValidJsonNumber(number))
    {
        auto       ok{false};
        const auto value{number.toDouble(&ok)};
        if (!ok)
            return std::nullopt;
        comparable.operand_ = QJsonValue{value};
    }
    else
    {
        return std::nullopt;
    }
    return comparable;
}

std::optional<QJsonValue> Comparable::evaluate(const QJsonValue& node) const
{
    if (const auto* query{std::get_if<SingularQuery>(&operand_)})
        return query->resolve(node);
    return std::get<QJsonValue>(operand_);
}

std::optional<ComparisonSplit> splitTopLevelComparison(QStringView expr)
{
    auto  depth{0};
    QChar quote{};
    for (qsizetype i{0}; i < expr.size(); ++i)
    {
        const auto c{expr[i]};
        if (!quote.isNull())
        {
            if (c == u'\\')
                ++i; // skip the escaped character
            else if (c == quote)
                quote = QChar{};
            continue;
        }
        if (c == u'\'' || c == u'"')
        {
            quote = c;
            continue;
        }
        if (c == u'(' || c == u'[')
        {
            ++depth;
            continue;
        }
        if (c == u')' || c == u']')
        {
            --depth;
            continue;
        }
        if (depth != 0)
            continue;

        const auto                        nextIsEq{i + 1 < expr.size() && expr[i + 1] == u'='};
        std::optional<ComparisonOperator> op{};
        if (c == u'=' && nextIsEq)
            op = ComparisonOperator::Equal;
        else if (c == u'!' && nextIsEq)
            op = ComparisonOperator::NotEqual;
        else if (c == u'<')
            op = nextIsEq ? ComparisonOperator::LessEqual : ComparisonOperator::Less;
        else if (c == u'>')
            op = nextIsEq ? ComparisonOperator::GreaterEqual : ComparisonOperator::Greater;
        if (!op)
            continue; // includes "!" (negation) and a lone "="

        const auto opLength{(nextIsEq || c == u'=' || c == u'!') ? 2 : 1};
        return ComparisonSplit{.lhs = expr.first(i).trimmed(), .op = *op, .rhs = expr.sliced(i + opLength).trimmed()};
    }
    return std::nullopt;
}

namespace
{

// RFC 9535 orders strings by Unicode scalar value; UTF-16 code-unit order
// differs only where a surrogate meets a BMP unit >= U+E000, so lift
// surrogates above the BMP before comparing.
[[nodiscard]] bool lessByScalarValue(QStringView lhs, QStringView rhs) noexcept
{
    const auto rank{[](char16_t c) noexcept -> char32_t
                    { return (c >= 0xD800 && c <= 0xDFFF) ? char32_t{c} + 0x10000 : char32_t{c}; }};
    const auto [l, r]{std::ranges::mismatch(lhs, rhs)};
    if (r == rhs.end())
        return false; // equal, or rhs is a prefix of lhs
    if (l == lhs.end())
        return true; // lhs is a proper prefix of rhs
    return rank(l->unicode()) < rank(r->unicode());
}

[[nodiscard]] bool equalValues(const std::optional<QJsonValue>& lhs, const std::optional<QJsonValue>& rhs)
{
    if (!lhs || !rhs)
        return !lhs && !rhs; // Nothing == Nothing only
    if (lhs->isDouble() && rhs->isDouble())
        return lhs->toDouble() == rhs->toDouble();
    return *lhs == *rhs; // same-type primitives; deep equality for arrays and objects
}

[[nodiscard]] bool lessValues(const std::optional<QJsonValue>& lhs, const std::optional<QJsonValue>& rhs)
{
    if (!lhs || !rhs)
        return false;
    if (lhs->isDouble() && rhs->isDouble())
        return lhs->toDouble() < rhs->toDouble();
    if (lhs->isString() && rhs->isString())
        return lessByScalarValue(lhs->toString(), rhs->toString());
    return false; // only numbers and strings are ordered
}

} // namespace

bool compareComparables(const std::optional<QJsonValue>& lhs,
                        ComparisonOperator               op,
                        const std::optional<QJsonValue>& rhs)
{
    switch (op)
    {
    case ComparisonOperator::Equal:
        return equalValues(lhs, rhs);
    case ComparisonOperator::NotEqual:
        return !equalValues(lhs, rhs);
    case ComparisonOperator::Less:
        return lessValues(lhs, rhs);
    case ComparisonOperator::LessEqual:
        return lessValues(lhs, rhs) || equalValues(lhs, rhs);
    case ComparisonOperator::Greater:
        return lessValues(rhs, lhs);
    case ComparisonOperator::GreaterEqual:
        return lessValues(rhs, lhs) || equalValues(lhs, rhs);
    }
    return false;
}

} // namespace json_query::json_path::detail
