// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "json-query/json-schema/JSONSchemaError.hpp"

#include <expected>
#include <QtCore/QString>
#include <QtCore/QStringView>

namespace json_query::json_schema::internal
{

/// Result type for format validation
using FormatValidationResult = std::expected<void, EvalError>;

// ---------------------------------------------------------------------------
// Format Validators (RFC-compliant format validation)
// ---------------------------------------------------------------------------

/// Validate date-time format (RFC 3339): YYYY-MM-DDTHH:MM:SSZ or with timezone
[[nodiscard]] FormatValidationResult isDateTime(QStringView value) noexcept;

/// Validate date format (RFC 3339): YYYY-MM-DD
[[nodiscard]] FormatValidationResult isDate(QStringView value) noexcept;

/// Validate time format (RFC 3339): HH:MM:SS with optional fractional seconds and timezone
[[nodiscard]] FormatValidationResult isTime(QStringView value) noexcept;

/// Validate email format (RFC 5321): local@domain
[[nodiscard]] FormatValidationResult isEmail(QStringView value) noexcept;

/// Validate hostname format (RFC 1123)
[[nodiscard]] FormatValidationResult isHostname(QStringView value) noexcept;

/// Validate IPv4 address: xxx.xxx.xxx.xxx where xxx is 0-255
[[nodiscard]] FormatValidationResult isIpv4(QStringView value) noexcept;

/// Validate IPv6 address (RFC 4291)
[[nodiscard]] FormatValidationResult isIpv6(QStringView value) noexcept;

/// Validate URI format (RFC 3986)
[[nodiscard]] FormatValidationResult isUri(QStringView value) noexcept;

/// Validate URI reference (RFC 3986): can be relative or absolute
[[nodiscard]] FormatValidationResult isUriReference(QStringView value) noexcept;

/// Validate URI template (RFC 6570)
[[nodiscard]] FormatValidationResult isUriTemplate(QStringView value) noexcept;

/// Validate UUID format (RFC 4122): xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
[[nodiscard]] FormatValidationResult isUuid(QStringView value) noexcept;

/// Validate JSON Pointer (RFC 6901)
[[nodiscard]] FormatValidationResult isJsonPointer(QStringView value) noexcept;

/// Validate relative JSON Pointer
[[nodiscard]] FormatValidationResult isRelativeJsonPointer(QStringView value) noexcept;

/// Validate regex format (ECMA-262)
[[nodiscard]] FormatValidationResult isRegex(QStringView value) noexcept;

/// Dispatch to appropriate validator based on format name
/// @param format The format name (e.g., "date-time", "email")
/// @param value The value to validate
/// @return Success if valid, EvalError::FormatInvalid if invalid
[[nodiscard]] FormatValidationResult validateFormat(QStringView format, QStringView value) noexcept;

} // namespace json_query::json_schema::internal
