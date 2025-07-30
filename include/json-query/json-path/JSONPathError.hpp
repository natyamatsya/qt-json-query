// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <QtCore/QString>
#include <cstdint>
#include <string_view>

namespace json_query::json_path
{

// Parse-time errors
enum class ParseError : std::uint8_t
{
    BlankInKey,
    EmptySegment,
    InvalidIdentifier, // RFC 9535: invalid member-name-shorthand
    InvalidIndex,
    InvalidSlice,
    MissingRoot,
    TrailingDot,
    TrailingRecursive,
    UnexpectedAfterRoot,
    UnmatchedBracket,
    UnmatchedQuote,
    UnsupportedFilter,
};

// Evaluation-time errors
enum class EvalError : std::uint8_t
{
    IndexOutOfRange,    // array index OOB
    InvalidSlice,       // invalid slice parameters (e.g., zero step)
    KeyNotFound,        // object key missing (for definite access)
    TypeMismatchArray,  // expected array but found other when index/slice
    TypeMismatchObject, // expected object but found other when key access
};

/**
 * @brief Convert a ParseError to a human-readable string view
 *
 * @param e The parse error to convert
 * @return QStringView A descriptive error message for the parse error
 */
[[nodiscard]] inline QStringView toQStringView(ParseError e) noexcept
{
    using enum ParseError;
    switch (e)
    {
    case BlankInKey:
        return QStringLiteral(
            "Syntax error: Blank or invalid character in member name (use quotes for names with special characters)");
    case EmptySegment:
        return QStringLiteral(
            "Syntax error: Empty segment between dots (consecutive '..' or segment with no identifier)");
    case InvalidIdentifier:
        return QStringLiteral(
            "Syntax error: Invalid member name identifier (must be a valid JSON string or JavaScript identifier)");
    case InvalidIndex:
        return QStringLiteral(
            "Syntax error: Invalid index-selector syntax (expected non-negative integer or comma-separated list)");
    case InvalidSlice:
        return QStringLiteral(
            "Syntax error: Invalid slice-selector syntax (expected [start:end:step] with optional values)");
    case MissingRoot:
        return QStringLiteral(
            "Syntax error: JSONPath must start with root identifier '$' (document root) or '@' (current node)");
    case TrailingDot:
        return QStringLiteral("Syntax error: Trailing '.' in segment (must be followed by a member name or wildcard)");
    case TrailingRecursive:
        return QStringLiteral(
            "Syntax error: Trailing '..' in descendant segment (must be followed by a member name or selector)");
    case UnexpectedAfterRoot:
        return QStringLiteral("Syntax error: Root identifier must be followed by '.' (dot) or '[' (bracket)");
    case UnmatchedBracket:
        return QStringLiteral("Syntax error: Unmatched '[' in selector (missing closing ']')");
    case UnmatchedQuote:
        return QStringLiteral("Syntax error: Unmatched quote in string literal (missing closing quote)");
    case UnsupportedFilter:
        return QStringLiteral("Syntax error: Unsupported or invalid filter-selector expression (check filter syntax)");
    default:
        return QStringLiteral("Unknown parse error");
    }
}

/**
 * @brief Convert a ParseError to a human-readable QString
 *
 * @param e The parse error to convert
 * @return QString A descriptive error message for the parse error
 */
[[nodiscard]] inline QString toQString(ParseError e) noexcept { return QString(toQStringView(e)); }

/**
 * @brief Convert an EvalError to a human-readable string view
 *
 * @param e The evaluation error to convert
 * @return QStringView A descriptive error message for the evaluation error
 */
[[nodiscard]] inline QStringView toQStringView(EvalError e) noexcept
{
    using enum EvalError;
    switch (e)
    {
    case IndexOutOfRange:
        return QStringLiteral("Range error: Array index exceeds the bounds of the target array");
    case InvalidSlice:
        return QStringLiteral("Range error: Invalid slice parameters (start, end, or step values are invalid)");
    case KeyNotFound:
        return QStringLiteral("Key error: The specified property does not exist in the target object");
    case TypeMismatchArray:
        return QStringLiteral("Type error: Cannot use array index or slice on non-array value (expected JSON array)");
    case TypeMismatchObject:
        return QStringLiteral("Type error: Cannot access property on non-object value (expected JSON object)");
    default:
        return QStringLiteral("Unknown evaluation error");
    }
}

/**
 * @brief Convert an EvalError to a human-readable QString
 *
 * @param e The evaluation error to convert
 * @return QString A descriptive error message for the evaluation error
 */
[[nodiscard]] inline QString toQString(EvalError e) noexcept { return QString(toQStringView(e)); }

} // namespace json_query::json_path
