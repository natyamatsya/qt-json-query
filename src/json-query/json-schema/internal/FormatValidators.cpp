// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-schema/internal/FormatValidators.hpp"

#include <QtCore/QRegularExpression>
#include <QtCore/QUrl>

namespace json_query::json_schema::internal
{

namespace
{

// Compile-time regex patterns for format validation
static const QRegularExpression dateTimePattern{
    u"^\\d{4}-\\d{2}-\\d{2}[Tt]\\d{2}:\\d{2}:\\d{2}(?:\\.\\d+)?(?:[Zz]|[+-]\\d{2}:\\d{2})$"_qs};

static const QRegularExpression datePattern{u"^\\d{4}-\\d{2}-\\d{2}$"_qs};

static const QRegularExpression timePattern{u"^\\d{2}:\\d{2}:\\d{2}(?:\\.\\d+)?(?:[Zz]|[+-]\\d{2}:\\d{2})?$"_qs};

static const QRegularExpression emailPattern{
    u"^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?(?:\\.[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?)*$"_qs};

static const QRegularExpression hostnamePattern{
    u"^(?:[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?\\.)*[a-zA-Z0-9](?:[a-zA-Z0-9-]{0,61}[a-zA-Z0-9])?$"_qs};

static const QRegularExpression ipv4Pattern{
    u"^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$"_qs};

static const QRegularExpression ipv6Pattern{
    u"^(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|^::(?:[0-9a-fA-F]{1,4}:){0,6}[0-9a-fA-F]{1,4}$|^[0-9a-fA-F]{1,4}::(?:[0-9a-fA-F]{1,4}:){0,5}[0-9a-fA-F]{1,4}$|^(?:[0-9a-fA-F]{1,4}:){2}:(?:[0-9a-fA-F]{1,4}:){0,4}[0-9a-fA-F]{1,4}$|^(?:[0-9a-fA-F]{1,4}:){3}:(?:[0-9a-fA-F]{1,4}:){0,3}[0-9a-fA-F]{1,4}$|^(?:[0-9a-fA-F]{1,4}:){4}:(?:[0-9a-fA-F]{1,4}:){0,2}[0-9a-fA-F]{1,4}$|^(?:[0-9a-fA-F]{1,4}:){5}:(?:[0-9a-fA-F]{1,4}:)?[0-9a-fA-F]{1,4}$|^(?:[0-9a-fA-F]{1,4}:){6}:[0-9a-fA-F]{1,4}$|^::$"_qs};

static const QRegularExpression uuidPattern{
    u"^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"_qs};

static const QRegularExpression jsonPointerPattern{u"^(?:/(?:[^~/]|~0|~1)*)*$"_qs};

static const QRegularExpression relativeJsonPointerPattern{u"^(?:0|[1-9][0-9]*)(?:#|(?:/(?:[^~/]|~0|~1)*)*)$"_qs};

} // anonymous namespace

bool FormatValidators::isDateTime(QStringView value) noexcept
{
    return dateTimePattern.match(value).hasMatch();
}

bool FormatValidators::isDate(QStringView value) noexcept
{
    return datePattern.match(value).hasMatch();
}

bool FormatValidators::isTime(QStringView value) noexcept
{
    const auto match{timePattern.match(value)};
    if (!match.hasMatch())
        return false;
    
    // Extract and validate hour, minute, second ranges
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
    return emailPattern.match(value).hasMatch();
}

bool FormatValidators::isHostname(QStringView value) noexcept
{
    if (value.isEmpty() || value.size() > 253)
        return false;
    return hostnamePattern.match(value).hasMatch();
}

bool FormatValidators::isIpv4(QStringView value) noexcept
{
    return ipv4Pattern.match(value).hasMatch();
}

bool FormatValidators::isIpv6(QStringView value) noexcept
{
    return ipv6Pattern.match(value).hasMatch();
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
    static const QRegularExpression templatePattern{u"^[^{}]*(?:\\{[^{}]+\\}[^{}]*)*$"_qs};
    return templatePattern.match(value).hasMatch();
}

bool FormatValidators::isUuid(QStringView value) noexcept
{
    return uuidPattern.match(value).hasMatch();
}

bool FormatValidators::isJsonPointer(QStringView value) noexcept
{
    return jsonPointerPattern.match(value).hasMatch();
}

bool FormatValidators::isRelativeJsonPointer(QStringView value) noexcept
{
    return relativeJsonPointerPattern.match(value).hasMatch();
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
