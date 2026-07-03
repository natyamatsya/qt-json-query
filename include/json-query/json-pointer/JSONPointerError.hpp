// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include <QtCore/QString>
#include <cstdint>
#include <string>
#include <string_view>

#include "../utils/detail/ErrorMap.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_pointer
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

// JSON Pointer parse error messages
inline constexpr auto json_pointer_parse_errors = utils::detail::ErrorMap<ParseError, 5>{
    {ParseError::ArrayIndexOverflow, DEFINE_ERROR_STRING("Array index is too large to be represented")},
    {ParseError::EmptyNonTerminalToken, DEFINE_ERROR_STRING("Non-terminal token in JSON Pointer cannot be empty")},
    {ParseError::InvalidEscapeSequence,
     DEFINE_ERROR_STRING("Invalid escape sequence in JSON Pointer (only ~0 and ~1 are valid)")},
    {ParseError::MissingLeadingSlash, DEFINE_ERROR_STRING("JSON Pointer must start with a leading slash")},
    {ParseError::NonDecimalArrayIndex,
     DEFINE_ERROR_STRING("Array index contains non-decimal characters or leading zeros")}};

// JSON Pointer evaluation error messages
inline constexpr auto json_pointer_eval_errors = utils::detail::ErrorMap<EvalError, 4>{
    {EvalError::IndexOutOfRange, DEFINE_ERROR_STRING("Array index out of range")},
    {EvalError::KeyNotFound, DEFINE_ERROR_STRING("Key not found in object")},
    {EvalError::TypeMismatchArray, DEFINE_ERROR_STRING("Type mismatch: expected array")},
    {EvalError::TypeMismatchObject, DEFINE_ERROR_STRING("Type mismatch: expected object")}};

/**
 * @brief Convert a ParseError to a human-readable string view
 *
 * @param e The parse error to convert
 * @return std::string_view A descriptive error message for the parse error
 */
[[nodiscard]] constexpr std::string_view to_std_sv(ParseError e) noexcept
{
    return json_pointer_parse_errors.get_std_sv(e);
}

/**
 * @brief Convert a ParseError to a QStringView
 *
 * @param e The parse error to convert
 * @return QStringView A view of the error message
 */
[[nodiscard]] constexpr QStringView to_qt_sv(ParseError e) noexcept { return json_pointer_parse_errors.get_qt_sv(e); }

/**
 * @brief Convert an EvalError to a human-readable string view
 *
 * @param e The evaluation error to convert
 * @return std::string_view A descriptive error message for the evaluation error
 */
[[nodiscard]] constexpr std::string_view to_std_sv(EvalError e) noexcept
{
    return json_pointer_eval_errors.get_std_sv(e);
}

/**
 * @brief Convert an EvalError to a QStringView
 *
 * @param e The evaluation error to convert
 * @return QStringView A view of the error message
 */
[[nodiscard]] constexpr QStringView to_qt_sv(EvalError e) noexcept { return json_pointer_eval_errors.get_qt_sv(e); }

} // namespace json_query::json_pointer
