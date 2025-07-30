// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <cstdint>
#include <string_view>

namespace json_query::json_path
{

// ------------------------------------------------------------------
//  Parser / compiler error codes
// ------------------------------------------------------------------
enum class ParseError : std::uint8_t
{
    Ok = 0, // not used in expected<T,E>
    MissingRoot,
    TrailingDot,
    TrailingRecursive,
    EmptySegment,
    BlankInKey,
    UnmatchedBracket,
    UnmatchedQuote,
    UnsupportedFilter,
    InvalidSlice,
    InvalidIndex,
    InvalidIdentifier, // RFC 9535: invalid member-name-shorthand
    UnexpectedAfterRoot
};

[[nodiscard]] inline constexpr std::string_view to_string(ParseError e) noexcept
{
    using enum Error;
    switch (e)
    {
    case MissingRoot:
        return "JSONPath must start with root identifier '$' or '@'";
    case TrailingDot:
        return "trailing '.' in segment";
    case TrailingRecursive:
        return "trailing '..' in descendant segment";
    case EmptySegment:
        return "empty segment";
    case BlankInKey:
        return "blank in member name";
    case UnmatchedBracket:
        return "unmatched '[' in selector";
    case UnmatchedQuote:
        return "unmatched quote in selector";
    case UnsupportedFilter:
        return "unsupported filter-selector expression";
    case InvalidSlice:
        return "invalid slice-selector syntax";
    case InvalidIndex:
        return "invalid index-selector syntax";
    case InvalidIdentifier:
        return "invalid member name identifier";
    case UnexpectedAfterRoot:
        return "root identifier must be followed by '.' or '['";
    default:
        return "unknown compilation error";
    }
}

// Evaluation-time error codes mirroring json_pointer::detail::EvalError.
// These are produced at runtime when a definite JSONPath encounters
// type or bounds violations as mandated by RFC 9535.
//
// Compilation/parse-time errors remain in json_path::Error.
//

enum class EvalError : std::uint8_t
{
    TypeMismatchObject = 0, // expected object but found other when key access
    TypeMismatchArray  = 1, // expected array but found other when index/slice
    KeyNotFound        = 2, // object key missing (for definite access)
    IndexOutOfRange    = 3, // array index OOB
    InvalidSlice       = 4  // invalid slice parameters (e.g., zero step)
};

[[nodiscard]] inline constexpr std::string_view to_string(EvalError e) noexcept
{
    using enum EvalError;
    switch (e)
    {
    case TypeMismatchObject:
        return "name-selector applied to non-object";
    case TypeMismatchArray:
        return "index-selector or slice-selector applied to non-array";
    case KeyNotFound:
        return "member name not found";
    case IndexOutOfRange:
        return "array index outside range";
    case InvalidSlice:
        return "invalid slice-selector parameters";
    default:
        return "unknown evaluation error";
    }
}

} // namespace json_query::json_path
