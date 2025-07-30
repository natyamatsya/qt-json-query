// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

namespace json_query::json_pointer
{

// Parse-time errors
enum class ParseError : std::uint8_t
{
    MissingLeadingSlash,
    EmptyNonTerminalToken,
    InvalidEscapeSequence,
    NonDecimalArrayIndex,
    ArrayIndexOverflow
};

// Evaluation-time errors
enum class EvalError : std::uint8_t
{
    TypeMismatchObject,
    TypeMismatchArray,
    KeyNotFound,
    IndexOutOfRange
};

[[nodiscard]] inline constexpr std::string_view to_string(EvalError e) noexcept
{
    using enum EvalError;
    switch (e)
    {
    case TypeMismatchObject:
        return "type mismatch: expected object";
    case TypeMismatchArray:
        return "type mismatch: expected array";
    case KeyNotFound:
        return "key not found in object";
    case IndexOutOfRange:
        return "array index out of range";
    default:
        return "unknown evaluation error";
    }
}

} // namespace json_query::json_pointer
