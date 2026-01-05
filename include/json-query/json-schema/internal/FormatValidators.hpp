// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "json-query/json-schema/JSONSchemaError.hpp"

#include <expected>
#include <QtCore/QString>
#include <QtCore/QStringView>

namespace json_query::json_schema::internal
{

/// Result type for format validation
using FormatResult = std::expected<void, EvalError>;

// ---------------------------------------------------------------------------
// Format Validators (RFC-compliant format validation)
// ---------------------------------------------------------------------------

/// Validate date-time format (RFC 3339): YYYY-MM-DDTHH:MM:SSZ or with timezone
[[nodiscard]] FormatResult isDateTime(QStringView value) noexcept;

/// Validate date format (RFC 3339): YYYY-MM-DD
[[nodiscard]] FormatResult isDate(QStringView value) noexcept;

/// Validate time format (RFC 3339): HH:MM:SS with optional fractional seconds and timezone
[[nodiscard]] FormatResult isTime(QStringView value) noexcept;

/// Validate email format (RFC 5321): local@domain
[[nodiscard]] FormatResult isEmail(QStringView value) noexcept;

/// Validate hostname format (RFC 1123)
[[nodiscard]] FormatResult isHostname(QStringView value) noexcept;

/// Validate IPv4 address: xxx.xxx.xxx.xxx where xxx is 0-255
[[nodiscard]] FormatResult isIpv4(QStringView value) noexcept;

/// Validate IPv6 address (RFC 4291)
[[nodiscard]] FormatResult isIpv6(QStringView value) noexcept;

/// Validate URI format (RFC 3986)
[[nodiscard]] FormatResult isUri(QStringView value) noexcept;

/// Validate URI reference (RFC 3986): can be relative or absolute
[[nodiscard]] FormatResult isUriReference(QStringView value) noexcept;

/// Validate URI template (RFC 6570)
[[nodiscard]] FormatResult isUriTemplate(QStringView value) noexcept;

/// Validate UUID format (RFC 4122): xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
[[nodiscard]] FormatResult isUuid(QStringView value) noexcept;

/// Validate JSON Pointer (RFC 6901)
[[nodiscard]] FormatResult isJsonPointer(QStringView value) noexcept;

/// Validate relative JSON Pointer
[[nodiscard]] FormatResult isRelativeJsonPointer(QStringView value) noexcept;

/// Validate regex format (ECMA-262)
[[nodiscard]] FormatResult isRegex(QStringView value) noexcept;

/// Dispatch to appropriate validator based on format name
/// @param format The format name (e.g., "date-time", "email")
/// @param value The value to validate
/// @return Success if valid, EvalError::FormatInvalid if invalid
[[nodiscard]] FormatResult validateFormat(QStringView format, QStringView value) noexcept;

} // namespace json_query::json_schema::internal

