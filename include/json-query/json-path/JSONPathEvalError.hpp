#pragma once

#include <cstdint>
#include <string_view>

namespace json_query::json_path {

// Evaluation-time error codes mirroring json_pointer::detail::EvalError.
// These are produced at runtime when a definite JSONPath encounters
// type or bounds violations as mandated by RFC 9535.
//
// Compilation/parse-time errors remain in json_path::Error.
//
// NOTE: Keep names and numeric values in sync with JSON Pointer to allow
// potential generic handling utilities.

enum class EvalError : std::uint8_t {
    TypeMismatchObject = 0, // expected object but found other when key access
    TypeMismatchArray  = 1, // expected array but found other when index/slice
    KeyNotFound        = 2, // object key missing (for definite access)
    IndexOutOfRange    = 3, // array index OOB
    InvalidSlice       = 4  // invalid slice parameters (e.g., zero step)
};

[[nodiscard]] inline constexpr std::string_view to_string(EvalError e) noexcept
{
    using enum EvalError;
    switch (e) {
    case TypeMismatchObject: return "type mismatch: expected object";
    case TypeMismatchArray : return "type mismatch: expected array";
    case KeyNotFound       : return "key not found";
    case IndexOutOfRange   : return "index out of range";
    case InvalidSlice      : return "invalid slice parameters";
    default                : return "unknown eval error";
    }
}

} // namespace json_query::json_path
