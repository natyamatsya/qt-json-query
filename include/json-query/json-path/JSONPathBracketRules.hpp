// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include <QStringView>
#include <QVector>
#include <QJsonValue>
#include <functional>
#include <expected>
#include <vector>

namespace json_query::json_path
{
// Forward declarations from JSONPathCompile.hpp
struct Token;
struct Slice;
// Using ParseError from JSONPathError.hpp instead of Error
enum class ParseError : std::uint8_t;

namespace detail
{
// Forward declaration for KeyBuilder
class KeyBuilder;

// ──────────────────────────────────────────────────────────────────────
//  BracketSink - Token Emission Facade
// ──────────────────────────────────────────────────────────────────────

/**
 * Facade for consistent token emission during bracket parsing
 * Handles bracket group metadata and filter management
 */
class BracketSink
{
  public:
    std::vector<Token>&                                                     tk;
    KeyBuilder&                                                             kb;
    std::vector<std::function<bool(const QJsonValue&, const QJsonValue&)>>& contextFilters;
    std::vector<std::function<bool(const QJsonValue&)>>&                    filters;
    int                                                                     currentBracketGroupId;

    BracketSink(std::vector<Token>&                                                     tokens,
                KeyBuilder&                                                             keyBuilder,
                std::vector<std::function<bool(const QJsonValue&, const QJsonValue&)>>& ctxFilters,
                std::vector<std::function<bool(const QJsonValue&)>>&                    filterFns,
                int                                                                     bracketGroupId) noexcept
        : tk(tokens),
          kb(keyBuilder),
          contextFilters(ctxFilters),
          filters(filterFns),
          currentBracketGroupId(bracketGroupId)
    {
    }

    // Token emission methods
    std::expected<void, ParseError> key(const QString& key, bool allow = false) const;
    void                            keyList(const std::vector<QString>& keys) const;
    void                            wild() const;
    void                            slice(const Slice& s) const;
    void                            index(int i) const;
    void                            pushFilter(const Token& t) const;
};

class EmbeddedBracketSink
{
  public:
    std::vector<Token>& tk;
    KeyBuilder&         kb;
    int                 currentBracketGroupId;

    EmbeddedBracketSink(std::vector<Token>& tokens, KeyBuilder& keyBuilder, int bracketGroupId)
        : tk(tokens), kb(keyBuilder), currentBracketGroupId(bracketGroupId)
    {
    }

    // Token emission methods
    std::expected<void, ParseError> key(const QString& key, bool allow = false) const;
    void                            keyList(const std::vector<QString>& keys) const;
    void                            wild() const;
    void                            slice(const Slice& s) const;
    void                            index(int i) const;
    void                            pushFilter(const Token& t) const;
};

// ──────────────────────────────────────────────────────────────────────
//  TableGen-Inspired Rule System Types
// ──────────────────────────────────────────────────────────────────────

// Function type aliases for rule system
// Note: These are used as function pointers in static arrays, so std::function works better
using BracketRuleMatcher         = std::function<bool(QStringView)>;
using BracketRuleHandler         = std::function<std::expected<void, ParseError>(QStringView, BracketSink&)>;
using EmbeddedBracketRuleHandler = std::function<std::expected<void, ParseError>(QStringView, EmbeddedBracketSink&)>;

/**
 * Declarative rule metadata structure (Legacy - with std::function storage)
 */
struct BracketRuleMetadata
{
    const char*        name;        // Human-readable rule name
    int                priority;    // Higher priority = checked first
    BracketRuleMatcher matcher;     // Pattern detection function
    BracketRuleHandler handler;     // Processing function
    const char*        description; // Documentation string
};

/**
 * Declarative rule metadata structure (Embedded-only - zero-overhead)
 */
struct EmbeddedBracketRuleMetadata
{
    const char*                name;        // Human-readable rule name
    int                        priority;    // Higher priority = checked first
    BracketRuleMatcher         matcher;     // Pattern detection function (same as legacy)
    EmbeddedBracketRuleHandler handler;     // Processing function (embedded-only)
    const char*                description; // Documentation string
};

// ──────────────────────────────────────────────────────────────────────
//  Rule Matcher Functions
// ──────────────────────────────────────────────────────────────────────

namespace matchers
{
bool matchesUnionComma(QStringView content);
bool matchesWildcard(QStringView content);
bool matchesSingleIndex(QStringView content);
bool matchesIndexList(QStringView content);
bool matchesSlice(QStringView content);
bool matchesFilterWithParens(QStringView content);
bool matchesFilterWithoutParens(QStringView content);
bool matchesPlaceholder(QStringView content);
bool matchesQuotedKey(QStringView content);
bool matchesUnquotedKey(QStringView content);
} // namespace matchers

// ──────────────────────────────────────────────────────────────────────
//  Rule Handler Functions (Legacy - with std::function storage)
// ──────────────────────────────────────────────────────────────────────

namespace handlers
{
std::expected<void, ParseError> handleUnionComma(QStringView content, BracketSink& out);
std::expected<void, ParseError> handleWildcard(QStringView content, BracketSink& out);
std::expected<void, ParseError> handleSingleIndex(QStringView content, BracketSink& out);
std::expected<void, ParseError> handleIndexList(QStringView content, BracketSink& out);
std::expected<void, ParseError> handleSlice(QStringView content, BracketSink& out);
std::expected<void, ParseError> handleFilterWithParens(QStringView content, BracketSink& out);
std::expected<void, ParseError> handleFilterWithoutParens(QStringView content, BracketSink& out);
std::expected<void, ParseError> handlePlaceholder(QStringView content, BracketSink& out);
std::expected<void, ParseError> handleQuotedKey(QStringView content, BracketSink& out);
std::expected<void, ParseError> handleUnquotedKey(QStringView content, BracketSink& out);
} // namespace handlers

// ──────────────────────────────────────────────────────────────────────
//  Embedded Rule Handler Functions (Zero-Overhead, no std::function storage)
// ──────────────────────────────────────────────────────────────────────

namespace embedded_handlers
{
std::expected<void, ParseError> handleUnionComma(QStringView content, EmbeddedBracketSink& out);
std::expected<void, ParseError> handleWildcard(QStringView content, EmbeddedBracketSink& out);
std::expected<void, ParseError> handleSingleIndex(QStringView content, EmbeddedBracketSink& out);
std::expected<void, ParseError> handleIndexList(QStringView content, EmbeddedBracketSink& out);
std::expected<void, ParseError> handleSlice(QStringView content, EmbeddedBracketSink& out);
std::expected<void, ParseError> handleFilterWithParens(QStringView content, EmbeddedBracketSink& out);
std::expected<void, ParseError> handleFilterWithoutParens(QStringView content, EmbeddedBracketSink& out);
std::expected<void, ParseError> handlePlaceholder(QStringView content, EmbeddedBracketSink& out);
std::expected<void, ParseError> handleQuotedKey(QStringView content, EmbeddedBracketSink& out);
std::expected<void, ParseError> handleUnquotedKey(QStringView content, EmbeddedBracketSink& out);
} // namespace embedded_handlers

// ──────────────────────────────────────────────────────────────────────
//  TableGen Rule Dispatcher
// ──────────────────────────────────────────────────────────────────────

/**
 * Declarative rule dispatcher using TableGen-inspired metadata
 */
class BracketRuleDispatcher
{
  private:
    // Initialize rules at runtime to avoid constexpr issues
    static std::vector<BracketRuleMetadata> createRules();

  public:
    // Get rules (initialized once)
    static const std::vector<BracketRuleMetadata>& getRules();

    // Main dispatch function using declarative rule table
    static std::expected<void, ParseError> dispatch(QStringView content, BracketSink& sink);

    // Helper for union processing to avoid recursion
    static std::expected<void, ParseError> processSegmentExcludingUnion(QStringView content, BracketSink& sink);

    // Utility function to get rule metadata for debugging/documentation
    static const BracketRuleMetadata* findRuleByName(const char* name);
};

class EmbeddedBracketRuleDispatcher
{
  public:
    static std::vector<EmbeddedBracketRuleMetadata>        createRules();
    static const std::vector<EmbeddedBracketRuleMetadata>& getRules();
    static std::expected<void, ParseError>                 dispatch(QStringView content, EmbeddedBracketSink& sink);
    static const EmbeddedBracketRuleMetadata*              findRuleByName(const char* name);
    static std::expected<void, ParseError>                 processSegmentExcludingUnion(QStringView          content,
                                                                                        EmbeddedBracketSink& sink);
};

std::expected<void, ParseError> parseBracket(QStringView content, BracketSink& sink);
std::expected<void, ParseError> parseBracket(QStringView content, EmbeddedBracketSink& sink);

} // namespace detail

} // namespace json_query::json_path
