// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-schema/internal/FormatValidators.hpp"

#include <ctre.hpp>
#include <QtCore/QDateTime>
#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>
#include <QtNetwork/QHostAddress>
#include "json-query/utils/JSONQueryUtils.hpp"

namespace json_query::json_schema::internal
{

namespace
{

// ============================================================================
// CTRE (Compile-Time Regular Expressions) Usage Notes
// ============================================================================
//
// CTRE implements most PCRE syntax. Key insights for pattern authoring:
//
// 1. RAW STRING LITERALS: Always use R"(...)" to avoid C++ escape issues
//    Example: R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}$)"
//
// 2. CHARACTER CLASS HYPHENS: Hyphen '-' in character classes is a range operator
//    - At START or END of class: literal hyphen (e.g., [-abc] or [abc-])
//    - Escaped with \-: literal hyphen anywhere (e.g., [a\-z] means a, -, z)
//    - CTRE explicitly allows: \- \" \< \> as character escapes
//
// 3. QString CONVERSION: Use utils::to_sv() to convert QString to std::string_view
//    CTRE works with std::string_view, not QStringView directly
//
// 4. PATTERN VALIDATION: CTRE validates patterns at compile-time
//    Invalid patterns cause compile errors with "problem_at_position<N>"
//
// 5. SEMANTIC VALIDATION: CTRE validates format, Qt validates semantics
//    Example: CTRE checks date format YYYY-MM-DD, Qt checks if date is valid
//
// References:
// - CTRE docs: https://compile-time-regular-expressions.readthedocs.io/
// - PCRE syntax: https://www.pcre.org/current/doc/html/pcre2syntax.html
// ============================================================================
namespace patterns
{
// Date/Time patterns (RFC 3339)
static constexpr ctll::fixed_string datePattern{R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}$)"};
static constexpr ctll::fixed_string dateTimePattern{
    R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}[Tt][0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?([Zz]|[\+\-][0-9]{2}:[0-9]{2})$)"};
static constexpr ctll::fixed_string timePattern{
    R"(^[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?([Zz]|[\+\-][0-9]{2}:[0-9]{2})?$)"};

// Email pattern (simplified RFC 5322) - use \- for literal hyphen
static constexpr ctll::fixed_string emailPattern{R"(^[a-zA-Z0-9._%\+\-]+@[a-zA-Z0-9][a-zA-Z0-9.\-]*[a-zA-Z0-9]$)"};

// Hostname pattern (RFC 1123) - each label: start/end alphanumeric, hyphens in middle
// Pattern: (label.)* label where label = [a-zA-Z0-9]([a-zA-Z0-9\-]*[a-zA-Z0-9])?
static constexpr ctll::fixed_string hostnamePattern{
    R"(^([a-zA-Z0-9]([a-zA-Z0-9\-]*[a-zA-Z0-9])?\.)*[a-zA-Z0-9]([a-zA-Z0-9\-]*[a-zA-Z0-9])?$)"};

// IPv4 pattern - simplified, Qt validates semantics
static constexpr ctll::fixed_string ipv4Pattern{R"(^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$)"};

// UUID pattern (RFC 4122)
static constexpr ctll::fixed_string uuidPattern{
    R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)"};

// JSON Pointer patterns (RFC 6901) - ~ must be followed by 0 or 1
static constexpr ctll::fixed_string jsonPointerPattern{R"(^(/([^~/]|~[01])*)*$)"};
static constexpr ctll::fixed_string relativeJsonPointerPattern{R"(^[0-9]+(#|(/([^~/]|~[01])*)*)$)"};

// URI template pattern (RFC 6570)
static constexpr ctll::fixed_string uriTemplatePattern{R"(^[^{}]*(\{[^{}]+\}[^{}]*)*$)"};
} // namespace patterns

// Helpers for returning differentiated format validation errors
inline constexpr auto formatInvalid{std::unexpected(EvalError::FormatInvalid)};           // Pattern mismatch
inline constexpr auto semanticInvalid{std::unexpected(EvalError::FormatSemanticInvalid)}; // Qt semantic check failed

} // anonymous namespace

FormatValidationResult isDateTime(QStringView value) noexcept
{
    // CTRE pattern match
    if (!ctre::match<patterns::dateTimePattern>(utils::to_sv(value.toString())))
        return formatInvalid;
    // Qt semantic validation (e.g., Feb 30 would fail here)
    const auto dt{QDateTime::fromString(value.toString(), Qt::ISODateWithMs)};
    if (!dt.isValid())
        return semanticInvalid;
    return {};
}

FormatValidationResult isDate(QStringView value) noexcept
{
    // CTRE pattern match
    if (!ctre::match<patterns::datePattern>(utils::to_sv(value.toString())))
        return formatInvalid;
    // Qt semantic validation (e.g., Feb 30 would fail here)
    const auto date{QDate::fromString(value.toString(), Qt::ISODate)};
    if (!date.isValid())
        return semanticInvalid;
    return {};
}

FormatValidationResult isTime(QStringView value) noexcept
{
    // CTRE pattern match
    if (!ctre::match<patterns::timePattern>(utils::to_sv(value.toString())))
        return formatInvalid;

    // Manual range validation (hour 0-23, minute 0-59, second 0-59)
    const auto str{value.toString()};
    if (str.length() < 8)
        return formatInvalid;

    bool       ok{};
    const auto hour{str.mid(0, 2).toInt(&ok)};
    if (!ok || hour > 23)
        return formatInvalid;

    const auto minute{str.mid(3, 2).toInt(&ok)};
    if (!ok || minute > 59)
        return formatInvalid;

    const auto second{str.mid(6, 2).toInt(&ok)};
    if (!ok || second > 59)
        return formatInvalid;

    return {};
}

FormatValidationResult isEmail(QStringView value) noexcept
{
    if (value.isEmpty() || value.size() > 254)
        return formatInvalid;
    if (!ctre::match<patterns::emailPattern>(utils::to_sv(value.toString())))
        return formatInvalid;
    return {};
}

FormatValidationResult isHostname(QStringView value) noexcept
{
    if (value.isEmpty() || value.size() > 253)
        return formatInvalid;
    if (!ctre::match<patterns::hostnamePattern>(utils::to_sv(value.toString())))
        return formatInvalid;
    return {};
}

FormatValidationResult isIpv4(QStringView value) noexcept
{
    // CTRE pattern match for format
    if (!ctre::match<patterns::ipv4Pattern>(utils::to_sv(value.toString())))
        return formatInvalid;
    // Qt validates the actual values (0-255 range)
    const QHostAddress addr{value.toString()};
    if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv4Protocol)
        return semanticInvalid;
    return {};
}

FormatValidationResult isIpv6(QStringView value) noexcept
{
    // Qt handles IPv6 validation (complex compression format)
    const QHostAddress addr{value.toString()};
    if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv6Protocol)
        return formatInvalid;
    return {};
}

FormatValidationResult isUri(QStringView value) noexcept
{
    // Qt handles URI validation
    const QUrl url{value.toString()};
    if (!url.isValid())
        return formatInvalid;
    if (url.scheme().isEmpty())
        return semanticInvalid; // Valid URI-reference but not absolute URI
    return {};
}

FormatValidationResult isUriReference(QStringView value) noexcept
{
    // Qt handles URI-reference validation
    const QUrl url{value.toString()};
    if (!url.isValid())
        return formatInvalid;
    return {};
}

FormatValidationResult isUriTemplate(QStringView value) noexcept
{
    if (!ctre::match<patterns::uriTemplatePattern>(utils::to_sv(value.toString())))
        return formatInvalid;
    return {};
}

FormatValidationResult isUuid(QStringView value) noexcept
{
    if (!ctre::match<patterns::uuidPattern>(utils::to_sv(value.toString())))
        return formatInvalid;
    return {};
}

FormatValidationResult isJsonPointer(QStringView value) noexcept
{
    if (!ctre::match<patterns::jsonPointerPattern>(utils::to_sv(value.toString())))
        return formatInvalid;
    return {};
}

FormatValidationResult isRelativeJsonPointer(QStringView value) noexcept
{
    if (!ctre::match<patterns::relativeJsonPointerPattern>(utils::to_sv(value.toString())))
        return formatInvalid;
    return {};
}

FormatValidationResult isRegex(QStringView value) noexcept
{
    // Qt validates regex syntax
    const QRegularExpression regex{value.toString()};
    if (!regex.isValid())
        return formatInvalid;
    return {};
}

// ────────────────────────────────────────────────────────────────────────────
// Format Dispatch Table
// ────────────────────────────────────────────────────────────────────────────

using FormatValidatorFn = FormatValidationResult (*)(QStringView) noexcept;

struct FormatEntry
{
    QStringView       name;
    FormatValidatorFn validator;
};

/// Compile-time dispatch table for format validators
static constexpr std::array kFormatTable{
    FormatEntry{u"date-time", isDateTime},
    FormatEntry{u"date", isDate},
    FormatEntry{u"time", isTime},
    FormatEntry{u"email", isEmail},
    FormatEntry{u"idn-email", isEmail},
    FormatEntry{u"hostname", isHostname},
    FormatEntry{u"idn-hostname", isHostname},
    FormatEntry{u"ipv4", isIpv4},
    FormatEntry{u"ipv6", isIpv6},
    FormatEntry{u"uri", isUri},
    FormatEntry{u"uri-reference", isUriReference},
    FormatEntry{u"iri", isUri},
    FormatEntry{u"iri-reference", isUriReference},
    FormatEntry{u"uri-template", isUriTemplate},
    FormatEntry{u"uuid", isUuid},
    FormatEntry{u"json-pointer", isJsonPointer},
    FormatEntry{u"relative-json-pointer", isRelativeJsonPointer},
    FormatEntry{u"regex", isRegex},
};

FormatValidationResult validateFormat(QStringView format, QStringView value) noexcept
{
    for (const auto& [name, validator] : kFormatTable)
        if (format == name)
            return validator(value);

    // Unknown format - pass validation (per JSON Schema spec)
    return {};
}

} // namespace json_query::json_schema::internal
