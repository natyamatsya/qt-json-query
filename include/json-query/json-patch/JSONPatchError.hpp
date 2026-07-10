// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include <QtCore/QString>
#include <cstdint>
#include <string>
#include <string_view>

#include "../utils/detail/ErrorMap.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_patch
{

// Parse-time errors (RFC 6902 §3: patch document structure)
enum class ParseError : std::uint8_t
{
    InvalidFromPointer,
    InvalidOperationName,
    InvalidPatchDocument,
    InvalidTargetPointer,
    MissingFrom,
    MissingOp,
    MissingPath,
    MissingValue,
};

// Apply-time errors specific to patch semantics. Failures of the underlying
// pointer operations keep their PointerEval domain and code; JSONPatch::apply
// only rewrites Error::detail to the failing operation index.
enum class EvalError : std::uint8_t
{
    MoveIntoOwnDescendant,
    TestFailed,
};

// JSON Patch parse error messages
inline constexpr auto json_patch_parse_errors = utils::detail::ErrorMap<ParseError, 8>{
    {ParseError::InvalidFromPointer, DEFINE_ERROR_STRING("Operation \"from\" member is not a valid JSON Pointer")},
    {ParseError::InvalidOperationName,
     DEFINE_ERROR_STRING("Operation \"op\" member must be one of add/remove/replace/move/copy/test")},
    {ParseError::InvalidPatchDocument,
     DEFINE_ERROR_STRING("A JSON Patch document must be an array of operation objects")},
    {ParseError::InvalidTargetPointer, DEFINE_ERROR_STRING("Operation \"path\" member is not a valid JSON Pointer")},
    {ParseError::MissingFrom, DEFINE_ERROR_STRING("move/copy operation requires a string \"from\" member")},
    {ParseError::MissingOp, DEFINE_ERROR_STRING("Operation object requires a string \"op\" member")},
    {ParseError::MissingPath, DEFINE_ERROR_STRING("Operation object requires a string \"path\" member")},
    {ParseError::MissingValue, DEFINE_ERROR_STRING("add/replace/test operation requires a \"value\" member")}};

// JSON Patch apply error messages
inline constexpr auto json_patch_eval_errors = utils::detail::ErrorMap<EvalError, 2>{
    {EvalError::MoveIntoOwnDescendant,
     DEFINE_ERROR_STRING("move operation cannot move a location into one of its own descendants")},
    {EvalError::TestFailed, DEFINE_ERROR_STRING("test operation failed: values are not equal")}};

/**
 * @brief Convert a ParseError to a human-readable string view
 *
 * @param e The parse error to convert
 * @return std::string_view A descriptive error message for the parse error
 */
[[nodiscard]] constexpr std::string_view to_std_sv(ParseError e) noexcept
{
    return json_patch_parse_errors.get_std_sv(e);
}

/**
 * @brief Convert a ParseError to a QStringView
 *
 * @param e The parse error to convert
 * @return QStringView A view of the error message
 */
[[nodiscard]] constexpr QStringView to_qt_sv(ParseError e) noexcept { return json_patch_parse_errors.get_qt_sv(e); }

/**
 * @brief Convert an EvalError to a human-readable string view
 *
 * @param e The apply error to convert
 * @return std::string_view A descriptive error message for the apply error
 */
[[nodiscard]] constexpr std::string_view to_std_sv(EvalError e) noexcept
{
    return json_patch_eval_errors.get_std_sv(e);
}

/**
 * @brief Convert an EvalError to a QStringView
 *
 * @param e The apply error to convert
 * @return QStringView A view of the error message
 */
[[nodiscard]] constexpr QStringView to_qt_sv(EvalError e) noexcept { return json_patch_eval_errors.get_qt_sv(e); }

} // namespace json_query::json_patch
