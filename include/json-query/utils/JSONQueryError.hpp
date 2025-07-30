// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

// Forward declare domain enums and their to_string functions
namespace json_query::json_path
{
// JSONPath parse/compile errors
enum class ParseError : std::uint8_t;
// JSONPath evaluation errors
enum class EvalError : std::uint8_t;

// String conversion functions
[[nodiscard]] constexpr std::string_view to_string(ParseError) noexcept;
[[nodiscard]] constexpr std::string_view to_string(EvalError) noexcept;
} // namespace json_query::json_path

namespace json_query::json_pointer
{
// JSON Pointer parse errors
enum class ParseError : std::uint8_t;
// JSON Pointer evaluation errors
enum class EvalError : std::uint8_t;

// String conversion functions
[[nodiscard]] constexpr std::string_view to_string(ParseError) noexcept;
[[nodiscard]] constexpr std::string_view to_string(EvalError) noexcept;
} // namespace json_query::json_pointer

// Conversion (QJsonValue -> T) errors live in this small enum
namespace json_query
{
enum class ConvertError : std::uint8_t
{
    TypeMismatch,
    NumericOutOfRange,
    NumericNotIntegral
};
inline constexpr std::string_view to_string(ConvertError e) noexcept
{
    using enum ConvertError;
    switch (e)
    {
    case TypeMismatch:
        return "type mismatch during conversion";
    case NumericOutOfRange:
        return "numeric conversion out of range";
    case NumericNotIntegral:
        return "expected integral number";
    }
    return "unknown conversion error";
}
} // namespace json_query

// -----------------------------------------------------------------------------

namespace json_query
{

enum class ErrorDomain : std::uint8_t
{
    PathParse,    // JSONPath parse errors
    PointerParse, // JSONPointer parse errors
    PathEval,     // JSONPath evaluation errors
    PointerEval,  // JSONPointer evaluation errors
    Convert       // General conversion errors
};

// Compile-time mapping from enum type -> domain
template <class E>
struct error_domain;

template <>
struct error_domain<json_path::ParseError>
{
    static constexpr ErrorDomain value = ErrorDomain::PathParse;
};
template <>
struct error_domain<json_pointer::ParseError>
{
    static constexpr ErrorDomain value = ErrorDomain::PointerParse;
};
template <>
struct error_domain<json_path::EvalError>
{
    static constexpr ErrorDomain value = ErrorDomain::PathEval;
};
template <>
struct error_domain<json_pointer::EvalError>
{
    static constexpr ErrorDomain value = ErrorDomain::PointerEval;
};
template <>
struct error_domain<ConvertError>
{
    static constexpr ErrorDomain value = ErrorDomain::Convert;
};

template <class E>
inline constexpr bool is_domain_enum_v =
    std::is_same_v<E, json_path::ParseError> || std::is_same_v<E, json_pointer::ParseError> ||
    std::is_same_v<E, json_path::EvalError> || std::is_same_v<E, json_pointer::EvalError> ||
    std::is_same_v<E, ConvertError>;

// The compact, unified error type
struct QueryError
{
    ErrorDomain  domain{};
    std::uint8_t code{};

    constexpr QueryError() = default;
    constexpr QueryError(ErrorDomain d, std::uint8_t c) : domain(d), code(c) {}

    // Explicit constructors for each domain error type
    constexpr explicit QueryError(json_path::ParseError e) noexcept
        : domain(ErrorDomain::PathParse), code(static_cast<std::uint8_t>(e))
    {
    }

    constexpr explicit QueryError(json_pointer::ParseError e) noexcept
        : domain(ErrorDomain::PointerParse), code(static_cast<std::uint8_t>(e))
    {
    }

    constexpr explicit QueryError(json_path::EvalError e) noexcept
        : domain(ErrorDomain::PathEval), code(static_cast<std::uint8_t>(e))
    {
    }

    constexpr explicit QueryError(json_pointer::EvalError e) noexcept
        : domain(ErrorDomain::PointerEval), code(static_cast<std::uint8_t>(e))
    {
    }

    constexpr explicit QueryError(ConvertError e) noexcept
        : domain(ErrorDomain::Convert), code(static_cast<std::uint8_t>(e))
    {
    }

    // Fallback template for any other enum type that maps to a domain
    template <class E>
        requires is_domain_enum_v<E>
    constexpr QueryError(E e) noexcept
        : domain(error_domain<E>::value), code(static_cast<std::uint8_t>(std::to_underlying(e)))
    {
    }

    // Stable numeric code: high byte = domain, low byte = enum's underlying value
    [[nodiscard]] constexpr std::uint16_t numeric() const noexcept
    {
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(domain) << 8) | code);
    }
    [[nodiscard]] static constexpr QueryError from_numeric(std::uint16_t n) noexcept
    {
        const auto d = static_cast<ErrorDomain>((n >> 8) & 0xFF);
        const auto c = static_cast<std::uint8_t>(n & 0xFF);
        return QueryError{d, c};
    }

    // Domain predicates
    [[nodiscard]] constexpr bool is_parse() const noexcept { return domain == ErrorDomain::Parse; }
    [[nodiscard]] constexpr bool is_path_eval() const noexcept { return domain == ErrorDomain::PathEval; }
    [[nodiscard]] constexpr bool is_pointer_eval() const noexcept { return domain == ErrorDomain::PointerEval; }
    [[nodiscard]] constexpr bool is_convert() const noexcept { return domain == ErrorDomain::Convert; }

    // Equality (so it works nicely in tests)
    friend constexpr bool operator==(QueryError a, QueryError b) noexcept
    {
        return a.domain == b.domain && a.code == b.code;
    }
    friend constexpr bool operator!=(QueryError a, QueryError b) noexcept { return !(a == b); }
};

static_assert(sizeof(QueryError) == 2, "QueryError should remain compact (2 bytes).");

/**
 * @brief Convert a QueryError to a human-readable string
 *
 * @param e The query error to convert
 * @return std::string_view A descriptive error message for the query error
 */
[[nodiscard]] inline constexpr std::string_view to_string(QueryError e) noexcept
{
    using enum ErrorDomain;
    switch (e.domain)
    {
    case PathParse:
        return json_path::to_string(static_cast<json_path::ParseError>(e.code));
    case PointerParse:
        return json_pointer::to_string(static_cast<json_pointer::ParseError>(e.code));
    case PathEval:
        return json_path::to_string(static_cast<json_path::EvalError>(e.code));
    case PointerEval:
        return json_pointer::to_string(static_cast<json_pointer::EvalError>(e.code));
    case Convert:
        return to_string(static_cast<ConvertError>(e.code));
    default:
        return "Unknown error domain";
    }
}

} // namespace json_query
