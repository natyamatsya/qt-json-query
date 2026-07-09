#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include <QtCore/QString>

#include "../utils/detail/ErrorMap.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path
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
    TooComplex,         // complexity or memory limit exceeded
    TypeMismatchArray,  // expected array but found other when index/slice
    TypeMismatchObject, // expected object but found other when key access
};

// JSON Path parse error messages
inline constexpr auto json_path_parse_errors = utils::detail::ErrorMap<ParseError, 12>{
    {ParseError::BlankInKey, DEFINE_ERROR_STRING("Blank key in member name")},
    {ParseError::EmptySegment, DEFINE_ERROR_STRING("Empty segment in path")},
    {ParseError::InvalidIdentifier, DEFINE_ERROR_STRING("Invalid identifier in path")},
    {ParseError::InvalidIndex, DEFINE_ERROR_STRING("Invalid array index")},
    {ParseError::InvalidSlice, DEFINE_ERROR_STRING("Invalid array slice")},
    {ParseError::MissingRoot, DEFINE_ERROR_STRING("Missing root identifier ($) at start of path")},
    {ParseError::TrailingDot, DEFINE_ERROR_STRING("Trailing dot in path")},
    {ParseError::TrailingRecursive, DEFINE_ERROR_STRING("Trailing recursive descent (..) in path")},
    {ParseError::UnexpectedAfterRoot, DEFINE_ERROR_STRING("Unexpected characters after root identifier")},
    {ParseError::UnmatchedBracket, DEFINE_ERROR_STRING("Unmatched bracket in path")},
    {ParseError::UnmatchedQuote, DEFINE_ERROR_STRING("Unmatched quote in string")},
    {ParseError::UnsupportedFilter, DEFINE_ERROR_STRING("Unsupported filter expression")}};

// JSON Path evaluation error messages
inline constexpr auto json_path_eval_errors = utils::detail::ErrorMap<EvalError, 6>{
    {EvalError::IndexOutOfRange, DEFINE_ERROR_STRING("Array index out of range")},
    {EvalError::InvalidSlice, DEFINE_ERROR_STRING("Invalid array slice parameters")},
    {EvalError::KeyNotFound, DEFINE_ERROR_STRING("Key not found in object")},
    {EvalError::TooComplex, DEFINE_ERROR_STRING("Complexity or memory limit exceeded")},
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
    return json_path_parse_errors.get_std_sv(e);
}

/**
 * @brief Convert a ParseError to a QStringView
 *
 * @param e The parse error to convert
 * @return QStringView A view of the error message
 */
[[nodiscard]] constexpr QStringView to_qt_sv(ParseError e) noexcept { return json_path_parse_errors.get_qt_sv(e); }

/**
 * @brief Convert an EvalError to a human-readable string view
 *
 * @param e The evaluation error to convert
 * @return std::string_view A descriptive error message for the evaluation error
 */
[[nodiscard]] constexpr std::string_view to_std_sv(EvalError e) noexcept
{
    return json_path_eval_errors.get_std_sv(e);
}

/**
 * @brief Convert an EvalError to a QStringView
 *
 * @param e The evaluation error to convert
 * @return QStringView A view of the error message
 */
[[nodiscard]] constexpr QStringView to_qt_sv(EvalError e) noexcept { return json_path_eval_errors.get_qt_sv(e); }

} // namespace json_query::inline JSON_QUERY_ABI_NS::json_path
