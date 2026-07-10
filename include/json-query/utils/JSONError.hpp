// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include "../json-patch/JSONPatchError.hpp"
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
#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS
{

// Alphabetical (ADR-004: numeric values are not API)
enum class ConvertError : std::uint8_t
{
    NumericNotIntegral,
    NumericOutOfRange,
    TypeMismatch
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
    case NumericNotIntegral:
        return "expected integral number";
    case NumericOutOfRange:
        return "numeric conversion out of range";
    case TypeMismatch:
        return "type mismatch during conversion";
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
    case NumericNotIntegral:
        return QStringLiteral("expected integral number");
    case NumericOutOfRange:
        return QStringLiteral("numeric conversion out of range");
    case TypeMismatch:
        return QStringLiteral("type mismatch during conversion");
    default:
        return QStringLiteral("unknown conversion error");
    }
}

// -----------------------------------------------------------------------------

// Grouped by module, parse before eval (ADR-004: numeric values are not API)
enum class ErrorDomain : std::uint8_t
{
    // --- JSONPath ---
    PathParse, // JSONPath parse errors
    PathEval,  // JSONPath evaluation errors

    // --- JSON Pointer ---
    PointerParse, // JSONPointer parse errors
    PointerEval,  // JSONPointer evaluation errors

    // --- JSON Patch ---
    PatchParse, // JSONPatch parse errors
    PatchEval,  // JSONPatch apply errors

    // --- JSON Schema ---
    SchemaParse, // JSON Schema parse/compilation errors
    SchemaEval,  // JSON Schema validation errors

    // --- Conversions ---
    Convert // General conversion errors
};

// ─────────────────────────────────────────────────────────────────────────
// The single enum-type -> ErrorDomain mapping. Everything else — the
// error_domain trait, the is_domain_enum_v concept, the Error constructor,
// and the message-dispatch switches — is generated from this list. Adding a
// module's error enum means adding ONE line here (plus the ErrorDomain
// enumerator above).
// ─────────────────────────────────────────────────────────────────────────
#define JSON_QUERY_ERROR_DOMAIN_LIST(X)                                                                               \
    X(json_path::ParseError, PathParse)                                                                               \
    X(json_path::EvalError, PathEval)                                                                                 \
    X(json_pointer::ParseError, PointerParse)                                                                         \
    X(json_pointer::EvalError, PointerEval)                                                                           \
    X(json_patch::ParseError, PatchParse)                                                                             \
    X(json_patch::EvalError, PatchEval)                                                                               \
    X(json_schema::ParseError, SchemaParse)                                                                           \
    X(json_schema::EvalError, SchemaEval)                                                                             \
    X(ConvertError, Convert)

// Compile-time mapping from enum type -> domain
template <class E>
struct error_domain;

#define JSON_QUERY_DEFINE_ERROR_DOMAIN(EnumType, DomainValue)                                                         \
    template <>                                                                                                       \
    struct error_domain<EnumType>                                                                                     \
    {                                                                                                                 \
        static constexpr ErrorDomain value = ErrorDomain::DomainValue;                                                \
    };
JSON_QUERY_ERROR_DOMAIN_LIST(JSON_QUERY_DEFINE_ERROR_DOMAIN)
#undef JSON_QUERY_DEFINE_ERROR_DOMAIN

// True exactly for the enums in JSON_QUERY_ERROR_DOMAIN_LIST (any type with
// an error_domain specialization)
template <class E, class = void>
inline constexpr bool is_domain_enum_v = false;
template <class E>
inline constexpr bool is_domain_enum_v<E, std::void_t<decltype(error_domain<E>::value)>> = true;

// The compact, unified error type
struct Error
{
    ErrorDomain   domain{};
    std::uint8_t  code{};
    std::uint16_t detail{}; // Context-dependent: token index for Path/Pointer eval errors, 0 = none

    constexpr Error() = default;
    constexpr Error(ErrorDomain d, std::uint8_t c, std::uint16_t detail = 0) : domain(d), code(c), detail(detail) {}

    // The one constructor for every domain error enum (the mapping comes
    // from JSON_QUERY_ERROR_DOMAIN_LIST via error_domain<E>). Implicit on
    // purpose: a bare enumerator has exactly one Error meaning.
    template <class E>
        requires is_domain_enum_v<E>
    constexpr Error(E e, std::uint16_t d = 0) noexcept
        : domain(error_domain<E>::value), code(static_cast<std::uint8_t>(std::to_underlying(e))), detail(d)
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
    [[nodiscard]] constexpr bool is_patch_parse() const noexcept { return domain == ErrorDomain::PatchParse; }
    [[nodiscard]] constexpr bool is_patch_eval() const noexcept { return domain == ErrorDomain::PatchEval; }
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
    switch (e.domain)
    {
        // Per-domain overloads resolve via ADL on the enum type
#define JSON_QUERY_TO_STD_SV_CASE(EnumType, DomainValue)                                                              \
    case ErrorDomain::DomainValue:                                                                                    \
        return to_std_sv(static_cast<EnumType>(e.code));
        JSON_QUERY_ERROR_DOMAIN_LIST(JSON_QUERY_TO_STD_SV_CASE)
#undef JSON_QUERY_TO_STD_SV_CASE
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
    switch (e.domain)
    {
        // Per-domain overloads resolve via ADL on the enum type
#define JSON_QUERY_TO_QT_SV_CASE(EnumType, DomainValue)                                                               \
    case ErrorDomain::DomainValue:                                                                                    \
        return to_qt_sv(static_cast<EnumType>(e.code));
        JSON_QUERY_ERROR_DOMAIN_LIST(JSON_QUERY_TO_QT_SV_CASE)
#undef JSON_QUERY_TO_QT_SV_CASE
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
    // Patch errors carry the operation index in detail (JSONPatch::apply also
    // rewrites the detail of propagated pointer errors to the op index — its
    // callers should prefer this formatting context).
    if (detail > 0 && (is_patch_parse() || is_patch_eval()))
        return QString(u"%1 (at operation %2)").arg(base).arg(detail);
    const auto hasTokenDetail{detail > 0 && (is_path_eval() || is_pointer_eval())};
    if (!hasTokenDetail)
        return base.toString();
    return QString(u"%1 (at token %2)").arg(base).arg(detail);
}

} // namespace json_query::inline JSON_QUERY_ABI_NS
