// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-path/JSONPathParsers.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathParseUtils.hpp"
#include "json-query/json-path/JSONPathBracketRules.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

#include <atomic>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path::detail
{

// ──────────────────────────────────────────────────────────────────────
//  Dot-Segment Parser Implementation
// ──────────────────────────────────────────────────────────────────────

std::expected<qsizetype, ParseError>
parseDot(qsizetype pos, QStringView sv, KeyBuilder& kb, std::vector<Token>& tokens)
{
    qCDebug(jsonPathLog) << "parseDot pos=" << pos;
    const auto n{sv.size()};
    if (++pos >= n)
        return std::unexpected(ParseError::TrailingDot);

    QChar nxt = sv[pos];
    qCDebug(jsonPathLog) << "parseDot: processing character '" << nxt << "' at pos=" << pos;

    if (nxt == u'.')
    {
        qCDebug(jsonPathLog) << "parseDot: found recursive segment (..)";
        tokens.emplace_back(Token{Token::Kind::Recursive});
        return pos + 2;
    }
    if (sv[pos] == u'*')
    {
        qCDebug(jsonPathLog) << "parseDot: found wildcard (*)";
        tokens.emplace_back(Token{Token::Kind::Wildcard});
        return pos + 1;
    }
    auto start{pos};
    while (pos < n && sv[pos] != u'.' && sv[pos] != u'[')
        ++pos;

    // '.' immediately followed by '[' (e.g. "$.[0]"): RFC 9535 §2.5.1 requires
    // "." to be followed by a wildcard or member-name-shorthand — and the
    // identifier below must not be empty (identifier[0] would read out of
    // bounds; found by fuzzing, visible only in assert-enabled builds).
    if (pos == start)
        return std::unexpected(ParseError::EmptySegment);

    // RFC 9535 validation: member-name-shorthand must follow ABNF grammar
    // name-first = ALPHA / "_" / Unicode, not DIGIT
    // name-char = name-first / DIGIT
    auto identifier{sv.sliced(start, pos - start)};

    // Check first character: must be ALPHA, underscore, or Unicode (not digit)
    QChar first = identifier[0];
    if (first.isDigit())
    {
        qCDebug(jsonPathLog) << "parseDot: rejecting numeric identifier" << identifier;
        return std::unexpected(ParseError::InvalidIdentifier);
    }
    if (!first.isLetter() && first != u'_' && first.unicode() < 0x80)
    {
        qCDebug(jsonPathLog) << "parseDot: rejecting invalid identifier" << identifier;
        return std::unexpected(ParseError::InvalidIdentifier);
    }

    // Check remaining characters: must be name-first or DIGIT
    for (qsizetype i = 1; i < identifier.size(); ++i)
    {
        QChar ch = identifier[i];
        if (!ch.isLetterOrNumber() && ch != u'_')
        {
            qCDebug(jsonPathLog) << "parseDot: rejecting identifier with invalid character" << identifier;
            return std::unexpected(ParseError::InvalidIdentifier);
        }
    }

    if (auto r = kb.push(identifier.toString()); !r)
        return std::unexpected(r.error());
    return pos;
}

// ──────────────────────────────────────────────────────────────────────
//  Bare Identifier Parser Implementation
// ──────────────────────────────────────────────────────────────────────

std::expected<qsizetype, ParseError>
parseBare(qsizetype pos, QStringView sv, KeyBuilder& kb, std::vector<Token>& tokens)
{
    auto start{pos};
    while (pos < sv.size() && sv[pos] != u'.' && sv[pos] != u'[')
        ++pos;
    if (pos == start)
        return std::unexpected(ParseError::EmptySegment);

    auto identifier{sv.sliced(start, pos - start)};

    // Special case: if the identifier is exactly '*', create a Wildcard token
    if (identifier == u"*")
    {
        kb.tgt.emplace_back(Token{Token::Kind::Wildcard});
        return pos;
    }

    // Otherwise, create a regular Key token
    if (auto r = kb.push(identifier.toString()); !r)
        return std::unexpected(r.error());
    return pos;
}

// ──────────────────────────────────────────────────────────────────────
//  Bracket Parser Implementation
// ──────────────────────────────────────────────────────────────────────

std::expected<qsizetype, ParseError> parseBracket(qsizetype                     pos,
                                                  QStringView                   sv,
                                                  KeyBuilder&                   kb,
                                                  std::vector<Token>&           tokens,
                                                  std::vector<ContextFilterFn>& contextFilters,
                                                  std::vector<FilterFn>&        filters)
{
    qCDebug(jsonPathLog) << "parseBracket: pos=" << pos << "sv.size()=" << sv.size();

    if (pos >= sv.size() || sv[pos] != u'[')
    {
        qCDebug(jsonPathLog) << "parseBracket: not a bracket at pos=" << pos;
        return std::unexpected(ParseError::UnmatchedBracket);
    }

    // Find matching closing bracket, handling nested brackets and quotes
    auto level{0};
    auto end{pos};
    auto inSingleQuote{false};
    auto inDoubleQuote{false};
    auto escaped{false};

    for (qsizetype i = pos; i < sv.size(); ++i)
    {
        QChar c = sv[i];

        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (c == u'\\')
        {
            escaped = true;
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote)
        {
            if (c == u'[')
            {
                level++;
            }
            else if (c == u']')
            {
                level--;
                if (level == 0)
                {
                    end = i;
                    break;
                }
            }
            else if (c == u'\'')
            {
                inSingleQuote = true;
            }
            else if (c == u'"')
            {
                inDoubleQuote = true;
            }
        }
        else if (inSingleQuote && c == u'\'')
        {
            inSingleQuote = false;
        }
        else if (inDoubleQuote && c == u'"')
        {
            inDoubleQuote = false;
        }
    }

    if (level != 0)
    {
        qCDebug(jsonPathLog) << "parseBracket: unmatched bracket";
        return std::unexpected(ParseError::UnmatchedBracket);
    }

    // Extract content between brackets
    auto content{sv.mid(pos + 1, end - pos - 1)};
    qCDebug(jsonPathLog) << "parseBracket: content=" << content.toString();

    // Generate unique bracket group ID for union tracking (atomic: create()
    // may be called from multiple threads concurrently)
    static std::atomic<int> nextBracketGroupId{1};
    auto currentBracketGroupId{nextBracketGroupId.fetch_add(1, std::memory_order_relaxed)};

    // Create BracketSink for token emission
    BracketSink sink(tokens, kb, contextFilters, filters, currentBracketGroupId);

    // Use TableGen rule dispatcher to process the bracket content
    auto result{BracketRuleDispatcher::dispatch(content, sink)};
    if (!result)
    {
        qCDebug(jsonPathLog) << "parseBracket: BracketRuleDispatcher::dispatch failed";
        return std::unexpected(result.error());
    }

    qCDebug(jsonPathLog) << "parseBracket: successfully processed bracket content";
    return end + 1; // Position after closing bracket
}

// ──────────────────────────────────────────────────────────────────────
//  Embedded-Only Bracket Parser Implementation (Zero-Overhead)
// ──────────────────────────────────────────────────────────────────────

std::expected<qsizetype, ParseError> parseEmbeddedBracket(
    qsizetype pos, QStringView sv, KeyBuilder& kb, std::vector<Token>& tokens, std::vector<FilterFn>& filterFns)
{
    qCDebug(jsonPathLog) << "parseEmbeddedBracket: pos=" << pos << "sv.size()=" << sv.size();

    if (pos >= sv.size() || sv[pos] != u'[')
    {
        qCDebug(jsonPathLog) << "parseEmbeddedBracket: not a bracket at pos=" << pos;
        return std::unexpected(ParseError::UnmatchedBracket);
    }

    // Find matching closing bracket, handling nested brackets and quotes
    auto level{0};
    auto end{pos};
    auto inSingleQuote{false};
    auto inDoubleQuote{false};
    auto escaped{false};

    for (qsizetype i = pos; i < sv.size(); ++i)
    {
        QChar c = sv[i];

        if (escaped)
        {
            escaped = false;
            continue;
        }

        if (c == u'\\')
        {
            escaped = true;
            continue;
        }

        if (!inSingleQuote && !inDoubleQuote)
        {
            if (c == u'[')
            {
                level++;
            }
            else if (c == u']')
            {
                level--;
                if (level == 0)
                {
                    end = i;
                    break;
                }
            }
            else if (c == u'\'')
            {
                inSingleQuote = true;
            }
            else if (c == u'"')
            {
                inDoubleQuote = true;
            }
        }
        else if (inSingleQuote && c == u'\'')
        {
            inSingleQuote = false;
        }
        else if (inDoubleQuote && c == u'"')
        {
            inDoubleQuote = false;
        }
    }

    if (level != 0)
    {
        qCDebug(jsonPathLog) << "parseEmbeddedBracket: unmatched bracket";
        return std::unexpected(ParseError::UnmatchedBracket);
    }

    // Extract content between brackets
    auto content{sv.mid(pos + 1, end - pos - 1)};
    qCDebug(jsonPathLog) << "parseEmbeddedBracket: content=" << content.toString();

    // Generate unique bracket group ID for union tracking (atomic: create()
    // may be called from multiple threads concurrently)
    static std::atomic<int> nextBracketGroupId{1};
    auto currentBracketGroupId{nextBracketGroupId.fetch_add(1, std::memory_order_relaxed)};

    // Create EmbeddedBracketSink for token emission (zero-overhead)
    EmbeddedBracketSink sink(tokens, kb, currentBracketGroupId);

    // Use embedded rule dispatcher to process the bracket content
    auto result{EmbeddedBracketRuleDispatcher::dispatch(content, sink)};
    if (!result)
    {
        qCDebug(jsonPathLog) << "parseEmbeddedBracket: EmbeddedBracketRuleDispatcher::dispatch failed";
        return std::unexpected(result.error());
    }

    qCDebug(jsonPathLog) << "parseEmbeddedBracket: successfully processed bracket content";
    return end + 1; // Position after closing bracket
}

} // namespace json_query::json_path::detail
