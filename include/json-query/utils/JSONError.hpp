// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include "../json-path/JSONPathError.hpp"
#include "../json-pointer/JSONPointerError.hpp"
#include "../json-schema/JSONSchemaError.hpp"

#include <QtCore/QString>
#include <QtCore/QStringView>

#include <cstdint>
#include <string_view>
#include <type_traits>
#include <utility>

// Conversion (QJsonValue -> T) errors live in this small enum
namespace json_query
{

enum class ConvertError : std::uint8_t
{
    TypeMismatch,
    NumericOutOfRange,
    NumericNotIntegral
};

/**
 * @brief Convert a ConvertError to a human-readable QStringView
 *
 * @param e The conversion error to convert
 * @return QStringView A view of a descriptive error message
 */
constexpr std::string_view to_std_sv(ConvertError e) noexcept
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
    default:
        return "unknown conversion error";
    }
}
/**
 * @brief Convert a ConvertError to a human-readable QStringView
 *
 * @param e The conversion error to convert
 * @return QStringView A view of a descriptive error message
 */
constexpr QStringView to_qt_sv(ConvertError e) noexcept
{
    using enum ConvertError;
    switch (e)
    {
    case TypeMismatch:
        return QStringLiteral("type mismatch during conversion");
    case NumericOutOfRange:
        return QStringLiteral("numeric conversion out of range");
    case NumericNotIntegral:
        return QStringLiteral("expected integral number");
    default:
        return QStringLiteral("unknown conversion error");
    }
}

// -----------------------------------------------------------------------------

enum class ErrorDomain : std::uint8_t
{
    PathParse,    // JSONPath parse errors
    PointerParse, // JSONPointer parse errors
    PathEval,     // JSONPath evaluation errors
    PointerEval,  // JSONPointer evaluation errors
    Convert,      // General conversion errors
    SchemaParse,  // JSON Schema parse/compilation errors
    SchemaEval    // JSON Schema validation errors
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
template <>
struct error_domain<json_schema::ParseError>
{
    static constexpr ErrorDomain value = ErrorDomain::SchemaParse;
};
template <>
struct error_domain<json_schema::EvalError>
{
    static constexpr ErrorDomain value = ErrorDomain::SchemaEval;
};

template <class E>
inline constexpr bool is_domain_enum_v =
    std::is_same_v<E, json_path::ParseError> || std::is_same_v<E, json_pointer::ParseError> ||
    std::is_same_v<E, json_path::EvalError> || std::is_same_v<E, json_pointer::EvalError> ||
    std::is_same_v<E, ConvertError> || std::is_same_v<E, json_schema::ParseError> ||
    std::is_same_v<E, json_schema::EvalError>;

// The compact, unified error type
struct Error
{
    ErrorDomain   domain{};
    std::uint8_t  code{};
    std::uint16_t detail{}; // Context-dependent: token index for Path/Pointer eval errors, 0 = none

    constexpr Error() = default;
    constexpr Error(ErrorDomain d, std::uint8_t c, std::uint16_t detail = 0) : domain(d), code(c), detail(detail) {}

    // Explicit constructors for each domain error type
    constexpr explicit Error(json_path::ParseError e, std::uint16_t d = 0) noexcept
        : domain(ErrorDomain::PathParse), code(static_cast<std::uint8_t>(e)), detail(d)
    {
    }

    constexpr explicit Error(json_pointer::ParseError e, std::uint16_t d = 0) noexcept
        : domain(ErrorDomain::PointerParse), code(static_cast<std::uint8_t>(e)), detail(d)
    {
    }

    constexpr explicit Error(json_path::EvalError e, std::uint16_t d = 0) noexcept
        : domain(ErrorDomain::PathEval), code(static_cast<std::uint8_t>(e)), detail(d)
    {
    }

    constexpr explicit Error(json_pointer::EvalError e, std::uint16_t d = 0) noexcept
        : domain(ErrorDomain::PointerEval), code(static_cast<std::uint8_t>(e)), detail(d)
    {
    }

    constexpr explicit Error(ConvertError e) noexcept
        : domain(ErrorDomain::Convert), code(static_cast<std::uint8_t>(e))
    {
    }

    constexpr explicit Error(json_schema::ParseError e) noexcept
        : domain(ErrorDomain::SchemaParse), code(static_cast<std::uint8_t>(e))
    {
    }

    constexpr explicit Error(json_schema::EvalError e) noexcept
        : domain(ErrorDomain::SchemaEval), code(static_cast<std::uint8_t>(e))
    {
    }

    // Fallback template for any other enum type that maps to a domain
    template <class E>
        requires is_domain_enum_v<E>
    constexpr Error(E e) noexcept
        : domain(error_domain<E>::value), code(static_cast<std::uint8_t>(std::to_underlying(e)))
    {
    }

    // Compact numeric code: high byte = domain, low byte = enum's underlying
    // value. NOT stable across library versions — enumerator values are an
    // implementation detail (see doc/adr/004). Do not persist or transmit.
    [[nodiscard]] constexpr std::uint16_t numeric() const noexcept
    {
        return static_cast<std::uint16_t>((static_cast<std::uint16_t>(domain) << 8) | code);
    }
    [[nodiscard]] static constexpr Error from_numeric(std::uint16_t n) noexcept
    {
        const auto d = static_cast<ErrorDomain>((n >> 8) & 0xFF);
        const auto c = static_cast<std::uint8_t>(n & 0xFF);
        return Error{d, c};
    }

    // Human-readable error message (defined after to_std_sv/to_qt_sv below)
    [[nodiscard]] constexpr std::string_view message() const noexcept;
    [[nodiscard]] constexpr QStringView      message_qt() const noexcept;

    // Rich formatted message including detail context (e.g. "Key not found (at token 2)")
    [[nodiscard]] inline QString formatted_message() const;

    // Domain predicates
    [[nodiscard]] constexpr bool is_path_parse() const noexcept { return domain == ErrorDomain::PathParse; }
    [[nodiscard]] constexpr bool is_pointer_parse() const noexcept { return domain == ErrorDomain::PointerParse; }
    [[nodiscard]] constexpr bool is_path_eval() const noexcept { return domain == ErrorDomain::PathEval; }
    [[nodiscard]] constexpr bool is_pointer_eval() const noexcept { return domain == ErrorDomain::PointerEval; }
    [[nodiscard]] constexpr bool is_convert() const noexcept { return domain == ErrorDomain::Convert; }
    [[nodiscard]] constexpr bool is_schema_parse() const noexcept { return domain == ErrorDomain::SchemaParse; }
    [[nodiscard]] constexpr bool is_schema_eval() const noexcept { return domain == ErrorDomain::SchemaEval; }

    // Equality — compares domain + code only (detail is context, not identity)
    friend constexpr bool operator==(Error a, Error b) noexcept { return a.domain == b.domain && a.code == b.code; }
    friend constexpr bool operator!=(Error a, Error b) noexcept { return !(a == b); }
};

static_assert(sizeof(Error) == 4, "Error should remain compact (4 bytes).");
/**
 * @brief Convert a Error to a human-readable string view
 *
 * @param e The query error to convert
 * @return std::string_view A descriptive error message for the evaluation error
 */
[[nodiscard]] constexpr std::string_view to_std_sv(Error e) noexcept
{
    using enum ErrorDomain;
    switch (e.domain)
    {
    case PathParse:
        return json_path::to_std_sv(static_cast<json_path::ParseError>(e.code));
    case PointerParse:
        return json_pointer::to_std_sv(static_cast<json_pointer::ParseError>(e.code));
    case PathEval:
        return json_path::to_std_sv(static_cast<json_path::EvalError>(e.code));
    case PointerEval:
        return json_pointer::to_std_sv(static_cast<json_pointer::EvalError>(e.code));
    case Convert:
        return to_std_sv(static_cast<ConvertError>(e.code));
    case SchemaParse:
        return json_schema::to_std_sv(static_cast<json_schema::ParseError>(e.code));
    case SchemaEval:
        return json_schema::to_std_sv(static_cast<json_schema::EvalError>(e.code));
    default:
        break;
    }
    return "unknown error domain";
}

/**
 * @brief Convert a Error to a human-readable QStringView
 *
 * @param e The query error to convert
 * @return QStringView A view of a descriptive error message
 */
[[nodiscard]] constexpr QStringView to_qt_sv(Error e) noexcept
{
    using enum ErrorDomain;
    switch (e.domain)
    {
    case PathParse:
        return json_path::to_qt_sv(static_cast<json_path::ParseError>(e.code));
    case PointerParse:
        return json_pointer::to_qt_sv(static_cast<json_pointer::ParseError>(e.code));
    case PathEval:
        return json_path::to_qt_sv(static_cast<json_path::EvalError>(e.code));
    case PointerEval:
        return json_pointer::to_qt_sv(static_cast<json_pointer::EvalError>(e.code));
    case Convert:
        return to_qt_sv(static_cast<ConvertError>(e.code));
    case SchemaParse:
        return json_schema::to_qt_sv(static_cast<json_schema::ParseError>(e.code));
    case SchemaEval:
        return json_schema::to_qt_sv(static_cast<json_schema::EvalError>(e.code));
    default:
        break;
    }
    return QStringLiteral("unknown error domain");
}

// Out-of-line definitions (depend on to_std_sv/to_qt_sv above)
constexpr std::string_view Error::message() const noexcept { return to_std_sv(*this); }
constexpr QStringView      Error::message_qt() const noexcept { return to_qt_sv(*this); }

inline QString Error::formatted_message() const
{
    const auto base{message_qt()};
    const auto hasTokenDetail{detail > 0 && (is_path_eval() || is_pointer_eval())};
    if (!hasTokenDetail)
        return base.toString();
    return QString(u"%1 (at token %2)").arg(base).arg(detail);
}

} // namespace json_query
