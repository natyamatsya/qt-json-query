// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include <QStringView>
#include <QString>
#include <vector>
#include <optional>
#include <expected>

namespace json_query::json_path
{
// Forward declarations from JSONPathCompile.hpp
struct Token;
struct Slice;
enum class ParseError : std::uint8_t;

// ──────────────────────────────────────────────────────────────────────
//  Parsing Utility Functions
// ──────────────────────────────────────────────────────────────────────

/**
 * Quote style for key validation
 */
enum class QuoteStyle
{
    Single,
    Double
};

/**
 * Parse slice notation (start:end:step) where parts may be empty
 * @param v String view containing slice notation
 * @return Parsed Slice structure or nullopt if invalid
 */
std::optional<Slice> makeSlice(QStringView v);

/**
 * Unescape quoted key according to RFC 9535 rules
 * @param key String view of the quoted key content
 * @return Unescaped key string
 */
QString unescapeQuotedKey(QStringView key);

/**
 * Validate quoted key according to RFC 9535 rules
 * @param key String view of the quoted key content
 * @param style Quote style (single or double quotes)
 * @return True if valid, false otherwise
 */
bool isValidQuotedKey(QStringView key, QuoteStyle style);

/**
 * Validate integer literals per RFC 9535 §4.2.3
 * @param content String view containing the integer literal
 * @return True if valid integer literal, false otherwise
 */
bool isValidIndexLiteral(QStringView content);

// ──────────────────────────────────────────────────────────────────────
//  KeyBuilder Utility Class
// ──────────────────────────────────────────────────────────────────────

namespace detail
{
/**
 * Helper class for building key tokens with validation
 */
class KeyBuilder
{
  public:
    std::vector<Token>& tgt;

    explicit KeyBuilder(std::vector<Token>& target) : tgt(target) {}

    /**
     * Add a key token with optional space validation
     * @param key The key string to add
     * @param allowSpace Whether to allow spaces in the key
     * @return Success or error result
     */
    std::expected<void, ParseError> push(QString key, bool allowSpace = false);
};
} // namespace detail

} // namespace json_query::json_path
