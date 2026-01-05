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
namespace patterns {
    // Date/Time patterns (RFC 3339)
    static constexpr ctll::fixed_string datePattern{R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}$)"};
    static constexpr ctll::fixed_string dateTimePattern{R"(^[0-9]{4}-[0-9]{2}-[0-9]{2}[Tt][0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?([Zz]|[\+\-][0-9]{2}:[0-9]{2})$)"};
    static constexpr ctll::fixed_string timePattern{R"(^[0-9]{2}:[0-9]{2}:[0-9]{2}(\.[0-9]+)?([Zz]|[\+\-][0-9]{2}:[0-9]{2})?$)"};
    
    // Email pattern (simplified RFC 5322) - use \- for literal hyphen
    static constexpr ctll::fixed_string emailPattern{R"(^[a-zA-Z0-9._%\+\-]+@[a-zA-Z0-9][a-zA-Z0-9.\-]*[a-zA-Z0-9]$)"};
    
    // Hostname pattern (RFC 1123) - each label: start/end alphanumeric, hyphens in middle
    // Pattern: (label.)* label where label = [a-zA-Z0-9]([a-zA-Z0-9\-]*[a-zA-Z0-9])?
    static constexpr ctll::fixed_string hostnamePattern{R"(^([a-zA-Z0-9]([a-zA-Z0-9\-]*[a-zA-Z0-9])?\.)*[a-zA-Z0-9]([a-zA-Z0-9\-]*[a-zA-Z0-9])?$)"};
    
    // IPv4 pattern - simplified, Qt validates semantics
    static constexpr ctll::fixed_string ipv4Pattern{R"(^[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}\.[0-9]{1,3}$)"};
    
    // UUID pattern (RFC 4122)
    static constexpr ctll::fixed_string uuidPattern{R"(^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$)"};
    
    // JSON Pointer patterns (RFC 6901) - ~ must be followed by 0 or 1
    static constexpr ctll::fixed_string jsonPointerPattern{R"(^(/([^~/]|~[01])*)*$)"};
    static constexpr ctll::fixed_string relativeJsonPointerPattern{R"(^[0-9]+(#|(/([^~/]|~[01])*)*)$)"};
    
    // URI template pattern (RFC 6570)
    static constexpr ctll::fixed_string uriTemplatePattern{R"(^[^{}]*(\{[^{}]+\}[^{}]*)*$)"};
}

} // anonymous namespace

bool FormatValidators::isDateTime(QStringView value) noexcept
{
    // CTRE pattern match
    if (!ctre::match<patterns::dateTimePattern>(utils::to_sv(value.toString())))
        return false;
    // Qt semantic validation
    const auto dt{QDateTime::fromString(value.toString(), Qt::ISODateWithMs)};
    return dt.isValid();
}

bool FormatValidators::isDate(QStringView value) noexcept
{
    // CTRE pattern match using utils::to_sv() helper
    if (!ctre::match<patterns::datePattern>(utils::to_sv(value.toString())))
        return false;
    // Qt semantic validation
    const auto date{QDate::fromString(value.toString(), Qt::ISODate)};
    return date.isValid();
}

bool FormatValidators::isTime(QStringView value) noexcept
{
    // CTRE pattern match
    if (!ctre::match<patterns::timePattern>(utils::to_sv(value.toString())))
        return false;
    
    // Manual range validation (hour 0-23, minute 0-59, second 0-59)
    const auto str{value.toString()};
    if (str.length() < 8)
        return false;
    
    bool ok{};
    const auto hour{str.mid(0, 2).toInt(&ok)};
    if (!ok || hour > 23)
        return false;
    
    const auto minute{str.mid(3, 2).toInt(&ok)};
    if (!ok || minute > 59)
        return false;
    
    const auto second{str.mid(6, 2).toInt(&ok)};
    if (!ok || second > 59)
        return false;
    
    return true;
}

bool FormatValidators::isEmail(QStringView value) noexcept
{
    if (value.isEmpty() || value.size() > 254)
        return false;
    return ctre::match<patterns::emailPattern>(utils::to_sv(value.toString()));
}

bool FormatValidators::isHostname(QStringView value) noexcept
{
    if (value.isEmpty() || value.size() > 253)
        return false;
    return ctre::match<patterns::hostnamePattern>(utils::to_sv(value.toString()));
}

bool FormatValidators::isIpv4(QStringView value) noexcept
{
    // CTRE pattern match for format
    if (!ctre::match<patterns::ipv4Pattern>(utils::to_sv(value.toString())))
        return false;
    // Qt validates the actual values (0-255 range)
    const QHostAddress addr{value.toString()};
    return !addr.isNull() && addr.protocol() == QAbstractSocket::IPv4Protocol;
}

bool FormatValidators::isIpv6(QStringView value) noexcept
{
    // Qt handles IPv6 well (complex compression format)
    const QHostAddress addr{value.toString()};
    return !addr.isNull() && addr.protocol() == QAbstractSocket::IPv6Protocol;
}

bool FormatValidators::isUri(QStringView value) noexcept
{
    const QUrl url{value.toString()};
    return url.isValid() && !url.scheme().isEmpty();
}

bool FormatValidators::isUriReference(QStringView value) noexcept
{
    const QUrl url{value.toString()};
    return url.isValid();
}

bool FormatValidators::isUriTemplate(QStringView value) noexcept
{
    return ctre::match<patterns::uriTemplatePattern>(utils::to_sv(value.toString()));
}

bool FormatValidators::isUuid(QStringView value) noexcept
{
    // CTRE pattern match
    return ctre::match<patterns::uuidPattern>(utils::to_sv(value.toString()));
}

bool FormatValidators::isJsonPointer(QStringView value) noexcept
{
    // CTRE pattern match using utils::to_sv() helper
    return ctre::match<patterns::jsonPointerPattern>(utils::to_sv(value.toString()));
}

bool FormatValidators::isRelativeJsonPointer(QStringView value) noexcept
{
    return ctre::match<patterns::relativeJsonPointerPattern>(utils::to_sv(value.toString()));
}

bool FormatValidators::isRegex(QStringView value) noexcept
{
    const QRegularExpression regex{value.toString()};
    return regex.isValid();
}

bool FormatValidators::validate(QStringView format, QStringView value) noexcept
{
    if (format == u"date-time")
        return isDateTime(value);
    if (format == u"date")
        return isDate(value);
    if (format == u"time")
        return isTime(value);
    if (format == u"email")
        return isEmail(value);
    if (format == u"idn-email")
        return isEmail(value);
    if (format == u"hostname")
        return isHostname(value);
    if (format == u"idn-hostname")
        return isHostname(value);
    if (format == u"ipv4")
        return isIpv4(value);
    if (format == u"ipv6")
        return isIpv6(value);
    if (format == u"uri")
        return isUri(value);
    if (format == u"uri-reference")
        return isUriReference(value);
    if (format == u"iri")
        return isUri(value);
    if (format == u"iri-reference")
        return isUriReference(value);
    if (format == u"uri-template")
        return isUriTemplate(value);
    if (format == u"uuid")
        return isUuid(value);
    if (format == u"json-pointer")
        return isJsonPointer(value);
    if (format == u"relative-json-pointer")
        return isRelativeJsonPointer(value);
    if (format == u"regex")
        return isRegex(value);

    return true;
}

} // namespace json_query::json_schema::internal
