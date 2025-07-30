// json-query/core/QueryError.hpp
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once
#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

// Forward declare your domain enums + their to_string functions
namespace json_query::json_path
{
enum class Error : std::uint8_t;     // parser/compile errors
enum class EvalError : std::uint8_t; // JSONPath evaluation errors

// You already have these:
std::string_view toString(Error) noexcept;      // note: toString (parser)
std::string_view to_string(EvalError) noexcept; // note: to_string (eval)
} // namespace json_query::json_path

namespace json_query::json_pointer::detail
{
enum class EvalError : std::uint8_t; // JSON Pointer evaluation errors
std::string_view to_string(EvalError) noexcept;
} // namespace json_query::json_pointer::detail

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
    Parse,
    PathEval,
    PointerEval,
    Convert
};

// Compile-time mapping from enum type -> domain
template <class E>
struct error_domain;

template <>
struct error_domain<json_path::Error>
{
    static constexpr ErrorDomain value = ErrorDomain::Parse;
};
template <>
struct error_domain<json_path::EvalError>
{
    static constexpr ErrorDomain value = ErrorDomain::PathEval;
};
template <>
struct error_domain<json_pointer::detail::EvalError>
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
    std::is_same_v<E, json_path::Error> || std::is_same_v<E, json_path::EvalError> ||
    std::is_same_v<E, json_pointer::EvalError> || std::is_same_v<E, ConvertError>;

// The compact, unified error type
struct QueryError
{
    ErrorDomain  domain{};
    std::uint8_t code{};

    constexpr QueryError() = default;
    constexpr QueryError(ErrorDomain d, std::uint8_t c) : domain(d), code(c) {}

    // Implicit construction from any known domain enum (typesafe; no macros)
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

// Message formatting — delegates to each domain's own to_string/toString
[[nodiscard]] inline constexpr std::string_view to_string(QueryError e) noexcept
{
    switch (e.domain)
    {
    case ErrorDomain::Parse:
        return json_path::toString(static_cast<json_path::Error>(e.code));
    case ErrorDomain::PathEval:
        return json_path::to_string(static_cast<json_path::EvalError>(e.code));
    case ErrorDomain::PointerEval:
        return json_pointer::detail::to_string(static_cast<json_pointer::detail::EvalError>(e.code));
    case ErrorDomain::Convert:
        return to_string(static_cast<ConvertError>(e.code));
    }
    return "unknown error";
}

} // namespace json_query
