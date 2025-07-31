// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QString>

#include <array>
#include <cstddef>
#include <string_view>

// Helper macro to define both UTF-8 and UTF-16 string literals
// This ensures each error message is only written once
#define DEFINE_ERROR_STRING(str) {str, u##str}

namespace json_query::detail
{

// Common error message strings that can be used across different error types
namespace error_messages
{

// Helper to define both UTF-8 and UTF-16 strings
struct ErrorString
{
    const char*     utf8;
    const char16_t* utf16;
};

// JSON Pointer error messages
inline constexpr ErrorString json_pointer_parse_errors[] = {
    DEFINE_ERROR_STRING("Array index is too large to be represented"),
    DEFINE_ERROR_STRING("Non-terminal token in JSON Pointer cannot be empty"),
    DEFINE_ERROR_STRING("Invalid escape sequence in JSON Pointer (only ~0 and ~1 are valid)"),
    DEFINE_ERROR_STRING("JSON Pointer must start with a leading slash"),
    DEFINE_ERROR_STRING("Array index contains non-decimal characters or leading zeros")};

inline constexpr ErrorString json_pointer_eval_errors[] = {DEFINE_ERROR_STRING("Array index out of range"),
                                                           DEFINE_ERROR_STRING("Key not found in object"),
                                                           DEFINE_ERROR_STRING("Type mismatch: expected array"),
                                                           DEFINE_ERROR_STRING("Type mismatch: expected object")};

// JSON Path error messages
inline constexpr ErrorString json_path_parse_errors[] = {
    DEFINE_ERROR_STRING("Blank key in member name"),
    DEFINE_ERROR_STRING("Empty segment in path"),
    DEFINE_ERROR_STRING("Invalid identifier in path"),
    DEFINE_ERROR_STRING("Invalid array index"),
    DEFINE_ERROR_STRING("Invalid array slice"),
    DEFINE_ERROR_STRING("Missing root identifier ($) at start of path"),
    DEFINE_ERROR_STRING("Trailing dot in path"),
    DEFINE_ERROR_STRING("Trailing recursive descent (..) in path"),
    DEFINE_ERROR_STRING("Unexpected characters after root identifier"),
    DEFINE_ERROR_STRING("Unmatched bracket in path"),
    DEFINE_ERROR_STRING("Unmatched quote in string"),
    DEFINE_ERROR_STRING("Unsupported filter expression")};

inline constexpr ErrorString json_path_eval_errors[] = {DEFINE_ERROR_STRING("Array index out of range"),
                                                        DEFINE_ERROR_STRING("Invalid array slice parameters"),
                                                        DEFINE_ERROR_STRING("Key not found in object"),
                                                        DEFINE_ERROR_STRING("Type mismatch: expected array"),
                                                        DEFINE_ERROR_STRING("Type mismatch: expected object")};

// Helper function to safely get an error message with bounds checking
template <typename Enum, size_t N>
[[nodiscard]] constexpr std::string_view get_message(Enum e, const std::array<ErrorString, N>& messages) noexcept
{
    const auto index = static_cast<std::underlying_type_t<Enum>>(e);
    return index < N ? std::string_view(messages[index].utf8) : "Unknown error";
}

// Helper function to get QString from error
template <typename Enum, size_t N>
[[nodiscard]] inline QString get_qstring(Enum e, const std::array<ErrorString, N>& messages) noexcept
{
    const auto index = static_cast<std::underlying_type_t<Enum>>(e);
    return index < N ? QString::fromUtf16(messages[index].utf16) : QStringLiteral("Unknown error");
}

} // namespace error_messages

} // namespace json_query::detail
