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

    const auto str{value.toString()};

    // Find the T/t separator to split date and time parts
    auto tPos{str.indexOf(u'T')};
    if (tPos < 0)
        tPos = str.indexOf(u't');
    if (tPos < 0)
        return formatInvalid;

    // Validate date part via Qt
    const auto datePart{str.left(tPos)};
    const auto date{QDate::fromString(datePart, Qt::ISODate)};
    if (!date.isValid())
        return semanticInvalid;

    // Validate time part (handles leap seconds that Qt rejects)
    const auto timePart{QStringView{str}.mid(tPos + 1)};
    return isTime(timePart);
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

    // Manual range validation (hour 0-23, minute 0-59, second 0-60 for leap seconds)
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
    if (!ok || second > 60) // 60 is valid for leap seconds
        return formatInvalid;

    // RFC 3339 requires a timezone offset (Z or +/-HH:MM) for full time values
    // Find where the time digits end (after seconds + optional fractional)
    auto pos{8}; // past HH:MM:SS
    if (pos < str.size() && str[pos] == u'.')
    {
        ++pos;
        while (pos < str.size() && str[pos].isDigit())
            ++pos;
    }
    // Must have timezone after the time value
    if (pos >= str.size())
        return formatInvalid; // No timezone offset
    const auto tzChar{str[pos]};
    if (tzChar != u'Z' && tzChar != u'z' && tzChar != u'+' && tzChar != u'-')
        return formatInvalid;

    // Parse timezone offset
    int tzOffsetMinutes{0};
    if (tzChar == u'+' || tzChar == u'-')
    {
        if (pos + 6 > str.size())
            return formatInvalid;
        const auto tzHour{str.mid(pos + 1, 2).toInt(&ok)};
        if (!ok || tzHour > 23)
            return formatInvalid;
        const auto tzMin{str.mid(pos + 4, 2).toInt(&ok)};
        if (!ok || tzMin > 59)
            return formatInvalid;
        tzOffsetMinutes = (tzHour * 60 + tzMin) * (tzChar == u'+' ? 1 : -1);
    }

    // Leap second (60) is valid only when UTC time is 23:59:60
    // UTC = local - offset
    if (second == 60)
    {
        const auto utcTotalMinutes{(hour * 60 + minute - tzOffsetMinutes + 24 * 60) % (24 * 60)};
        const auto utcHour{utcTotalMinutes / 60};
        const auto utcMinute{utcTotalMinutes % 60};
        if (utcHour != 23 || utcMinute != 59)
            return formatInvalid;
    }

    return {};
}

FormatValidationResult isEmail(QStringView value) noexcept
{
    const auto str{value.toString()};
    if (str.isEmpty() || str.size() > 254)
        return formatInvalid;

    // Find the last @ to split local and domain
    const auto atPos{str.lastIndexOf(u'@')};
    if (atPos < 1 || atPos >= str.size() - 1)
        return formatInvalid;

    const auto local{QStringView{str}.left(atPos)};
    const auto domain{QStringView{str}.mid(atPos + 1)};

    // Validate local part
    if (local.isEmpty() || local.size() > 64)
        return formatInvalid;

    if (local.startsWith(u'"') && local.endsWith(u'"'))
    {
        // Quoted local part — most printable ASCII allowed inside quotes
        if (local.size() < 2)
            return formatInvalid;
    }
    else
    {
        // Unquoted local part: atext characters + dots (no leading/trailing/consecutive dots)
        if (local.startsWith(u'.') || local.endsWith(u'.'))
            return formatInvalid;
        bool prevDot{false};
        for (const auto ch : local)
        {
            if (ch == u'.')
            {
                if (prevDot)
                    return formatInvalid; // consecutive dots
                prevDot = true;
                continue;
            }
            prevDot = false;
            // RFC 5321 atext: alphanumeric + !#$%&'*+/=?^_`{|}~-
            if (ch.isLetterOrNumber())
                continue;
            static constexpr QStringView atext{u"!#$%&'*+/=?^_`{|}~-"};
            if (!atext.contains(ch))
                return formatInvalid;
        }
    }

    // Validate domain part
    if (domain.startsWith(u'[') && domain.endsWith(u']'))
    {
        // IP-literal domain: [IPv4] or [IPv6:addr]
        const auto inner{domain.mid(1, domain.size() - 2)};
        if (inner.startsWith(u"IPv6:"))
        {
            const QHostAddress addr{inner.mid(5).toString()};
            if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv6Protocol)
                return formatInvalid;
        }
        else
        {
            const QHostAddress addr{inner.toString()};
            if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv4Protocol)
                return formatInvalid;
        }
        return {};
    }

    // Standard domain: validate as hostname
    return isHostname(domain);
}

FormatValidationResult isHostname(QStringView value) noexcept
{
    if (value.isEmpty() || value.size() > 253)
        return formatInvalid;
    if (!ctre::match<patterns::hostnamePattern>(utils::to_sv(value.toString())))
        return formatInvalid;
    // RFC 1123: each label must be 1-63 characters
    // RFC 5891 §4.2.3.1: labels must not have "--" at positions 3-4
    const auto str{value.toString()};
    const auto labels = str.split(u'.');
    for (const auto& label : labels)
    {
        if (label.isEmpty() || label.size() > 63)
            return semanticInvalid;
        if (label.size() >= 4 && label[2] == u'-' && label[3] == u'-')
            return semanticInvalid;
    }
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
    const auto str{value.toString()};
    // Reject leading/trailing whitespace and zone IDs (%25...)
    if (str.isEmpty() || str[0].isSpace() || str.back().isSpace())
        return formatInvalid;
    if (str.contains(u'%'))
        return formatInvalid;
    // Qt handles IPv6 validation (complex compression format)
    const QHostAddress addr{str};
    if (addr.isNull() || addr.protocol() != QAbstractSocket::IPv6Protocol)
        return formatInvalid;
    return {};
}

namespace
{
/// RFC 3986 §2.1: reject characters not allowed in URIs
/// Forbidden: space, <, >, ", {, }, |, \, ^, `, and non-ASCII
[[nodiscard]] bool containsInvalidUriChars(const QString& str)
{
    for (const auto ch : str)
    {
        if (ch.unicode() > 127)
            return true;
        switch (ch.unicode())
        {
        case ' ':
        case '<':
        case '>':
        case '"':
        case '{':
        case '}':
        case '|':
        case '\\':
        case '^':
        case '`':
            return true;
        default:
            break;
        }
    }
    return false;
}
} // namespace

FormatValidationResult isUri(QStringView value) noexcept
{
    const auto str{value.toString()};
    if (containsInvalidUriChars(str))
        return formatInvalid;
    // Qt handles URI validation
    const QUrl url{str, QUrl::StrictMode};
    if (!url.isValid())
        return formatInvalid;
    if (url.scheme().isEmpty())
        return semanticInvalid; // Valid URI-reference but not absolute URI
    return {};
}

FormatValidationResult isUriReference(QStringView value) noexcept
{
    const auto str{value.toString()};
    if (containsInvalidUriChars(str))
        return formatInvalid;
    // Qt handles URI-reference validation
    const QUrl url{str, QUrl::StrictMode};
    if (!url.isValid())
        return formatInvalid;
    return {};
}

FormatValidationResult isIri(QStringView value) noexcept
{
    // IRIs (RFC 3987) allow non-ASCII but still forbid control characters
    const auto str{value.toString()};
    for (const auto ch : str)
    {
        const auto cp{ch.unicode()};
        if (cp < 0x20 || cp == 0x7F) // control characters
            return formatInvalid;
        switch (cp)
        {
        case ' ':
        case '<':
        case '>':
        case '"':
        case '{':
        case '}':
        case '|':
        case '\\':
        case '^':
        case '`':
            return formatInvalid;
        default:
            break;
        }
    }
    const QUrl url{str, QUrl::StrictMode};
    if (!url.isValid())
        return formatInvalid;
    if (url.scheme().isEmpty())
        return semanticInvalid;
    return {};
}

FormatValidationResult isIriReference(QStringView value) noexcept
{
    const auto str{value.toString()};
    for (const auto ch : str)
    {
        const auto cp{ch.unicode()};
        if (cp < 0x20 || cp == 0x7F)
            return formatInvalid;
        switch (cp)
        {
        case ' ':
        case '<':
        case '>':
        case '"':
        case '{':
        case '}':
        case '|':
        case '\\':
        case '^':
        case '`':
            return formatInvalid;
        default:
            break;
        }
    }
    const QUrl url{str, QUrl::StrictMode};
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
    // Leading zeros are not allowed (e.g., "01" is invalid, but "0" is valid)
    const auto str{value.toString()};
    if (str.size() >= 2 && str[0] == u'0' && str[1].isDigit())
        return formatInvalid;
    return {};
}

FormatValidationResult isDuration(QStringView value) noexcept
{
    // ISO 8601 duration: P[nY][nM][nD][T[nH][nM][nS]] or P[n]W
    const auto str{value.toString()};
    if (str.isEmpty() || str[0] != u'P')
        return formatInvalid;

    // Only ASCII characters allowed (reject non-ASCII digits like Bengali ২)
    for (const auto ch : str)
        if (ch.unicode() > 127)
            return formatInvalid;

    auto pos{1}; // past 'P'
    if (pos >= str.size())
        return formatInvalid; // "P" alone is invalid

    // Week duration: P[n]W — cannot be combined with other units
    if (str.contains(u'W'))
    {
        // Must be exactly P<digits>W
        auto numStart{pos};
        while (pos < str.size() && str[pos].isDigit())
            ++pos;
        if (pos == numStart || pos >= str.size() || str[pos] != u'W')
            return formatInvalid;
        ++pos;
        return pos == str.size() ? FormatValidationResult{} : formatInvalid;
    }

    // Helper: try to parse [nU] components in order, skipping absent ones
    auto parseComponents = [&](const QChar* units, int unitCount, bool allowFrac) -> bool
    {
        bool found{false};
        int  ui{0};
        while (ui < unitCount && pos < str.size())
        {
            if (!str[pos].isDigit())
                break;
            const auto numStart{pos};
            while (pos < str.size() && str[pos].isDigit())
                ++pos;
            // Allow fractional part on last component if enabled
            if (allowFrac && pos < str.size() && str[pos] == u'.')
            {
                ++pos;
                while (pos < str.size() && str[pos].isDigit())
                    ++pos;
            }
            if (pos >= str.size())
            {
                pos = numStart;
                break;
            }
            // Find which unit this matches (must be at or after current ui)
            bool matched{false};
            for (int j{ui}; j < unitCount; ++j)
            {
                if (str[pos] == units[j])
                {
                    ++pos;
                    ui = j + 1;
                    found = true;
                    matched = true;
                    break;
                }
            }
            if (!matched)
            {
                pos = numStart;
                break;
            }
        }
        return found;
    };

    // Date part: [nY][nM][nD]
    static constexpr QChar dateUnits[]{u'Y', u'M', u'D'};
    const auto hasDatePart{parseComponents(dateUnits, 3, false)};

    // Time part: T[nH][nM][nS]
    bool hasTimePart{false};
    if (pos < str.size() && str[pos] == u'T')
    {
        ++pos;
        if (pos >= str.size())
            return formatInvalid; // "PT" or "P...T" with nothing after T

        static constexpr QChar timeUnits[]{u'H', u'M', u'S'};
        hasTimePart = parseComponents(timeUnits, 3, true);
    }

    if (!hasDatePart && !hasTimePart)
        return formatInvalid;
    if (pos != str.size())
        return formatInvalid; // Trailing characters

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
    FormatEntry{u"iri", isIri},
    FormatEntry{u"iri-reference", isIriReference},
    FormatEntry{u"uri-template", isUriTemplate},
    FormatEntry{u"uuid", isUuid},
    FormatEntry{u"json-pointer", isJsonPointer},
    FormatEntry{u"relative-json-pointer", isRelativeJsonPointer},
    FormatEntry{u"regex", isRegex},
    FormatEntry{u"duration", isDuration},
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
