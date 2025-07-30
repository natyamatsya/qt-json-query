// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QString>
#include <string_view>

namespace json_query::json_pointer
{

// Parse-time errors
enum class ParseError : std::uint8_t
{
    ArrayIndexOverflow,
    EmptyNonTerminalToken,
    InvalidEscapeSequence,
    MissingLeadingSlash,
    NonDecimalArrayIndex,
};

// Evaluation-time errors
enum class EvalError : std::uint8_t
{
    IndexOutOfRange,
    KeyNotFound,
    TypeMismatchArray,
    TypeMismatchObject,
};

/**
 * @brief Convert a ParseError to a human-readable string
 *
 * @param e The evaluation error to convert
 * @return std::string_view A descriptive error message for the evaluation error
 */
[[nodiscard]] inline constexpr std::string_view to_string(ParseError e) noexcept
{
    using enum ParseError;
    switch (e)
    {
    case ArrayIndexOverflow:
        return "Array index is too large to be represented";
    case EmptyNonTerminalToken:
        return "Non-terminal token in JSON Pointer cannot be empty";
    case InvalidEscapeSequence:
        return "Invalid escape sequence in JSON Pointer (only ~0 and ~1 are valid)";
    case MissingLeadingSlash:
        return "JSON Pointer must start with a leading slash";
    case NonDecimalArrayIndex:
        return "Array index contains non-decimal characters or leading zeros";
    default:
        return "Unknown parse error";
    }
}

/**
 * @brief Convert a ParseError to a human-readable QStringView
 *
 * @param e The parse error to convert
 * @return QStringView A view of a descriptive error message
 */
[[nodiscard]] inline QStringView toQStringView(ParseError e) noexcept
{
    using enum ParseError;
    switch (e)
    {
    case ArrayIndexOverflow:
        return QStringLiteral("Array index is too large to be represented");
    case EmptyNonTerminalToken:
        return QStringLiteral("Non-terminal token in JSON Pointer cannot be empty");
    case InvalidEscapeSequence:
        return QStringLiteral("Invalid escape sequence in JSON Pointer (only ~0 and ~1 are valid)");
    case MissingLeadingSlash:
        return QStringLiteral("JSON Pointer must start with a leading slash");
    case NonDecimalArrayIndex:
        return QStringLiteral("Array index contains non-decimal characters or leading zeros");
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
 * @brief Convert an EvalError to a human-readable string
 *
 * @param e The evaluation error to convert
 * @return std::string_view A descriptive error message for the evaluation error
 */
[[nodiscard]] inline constexpr std::string_view to_string(EvalError e) noexcept
{
    using enum EvalError;
    switch (e)
    {
    case IndexOutOfRange:
        return "Index out of range: Array index exceeds the bounds of the target array";
    case KeyNotFound:
        return "Key not found: The specified property does not exist in the target object";
    case TypeMismatchArray:
        return "Type mismatch: Cannot use array index on non-array value (expected JSON array)";
    case TypeMismatchObject:
        return "Type mismatch: Cannot access property on non-object value (expected JSON object)";
    default:
        return "Unknown evaluation error";
    }
}

/**
 * @brief Convert an EvalError to a human-readable QStringView
 *
 * @param e The evaluation error to convert
 * @return QStringView A view of a descriptive error message
 */
[[nodiscard]] inline QStringView toQStringView(EvalError e) noexcept
{
    using enum EvalError;
    switch (e)
    {
    case IndexOutOfRange:
        return QStringLiteral("Index out of range: Array index exceeds the bounds of the target array");
    case KeyNotFound:
        return QStringLiteral("Key not found: The specified property does not exist in the target object");
    case TypeMismatchArray:
        return QStringLiteral("Type mismatch: Cannot use array index on non-array value (expected JSON array)");
    case TypeMismatchObject:
        return QStringLiteral("Type mismatch: Cannot access property on non-object value (expected JSON object)");
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

} // namespace json_query::json_pointer
