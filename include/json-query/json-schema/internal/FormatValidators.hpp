// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QString>
#include <QtCore/QStringView>

namespace json_query::json_schema::internal
{

/**
 * @brief Format validators for the 'format' keyword
 *
 * Implements common format validators as defined in JSON Schema specification.
 * These are validation assertions that may be used by implementations.
 */
class FormatValidators
{
  public:
    /**
     * @brief Validate date-time format (RFC 3339)
     * Format: YYYY-MM-DDTHH:MM:SSZ or with timezone offset
     */
    [[nodiscard]] static bool isDateTime(QStringView value) noexcept;

    /**
     * @brief Validate date format (RFC 3339)
     * Format: YYYY-MM-DD
     */
    [[nodiscard]] static bool isDate(QStringView value) noexcept;

    /**
     * @brief Validate time format (RFC 3339)
     * Format: HH:MM:SS or HH:MM:SS.sss with optional timezone
     */
    [[nodiscard]] static bool isTime(QStringView value) noexcept;

    /**
     * @brief Validate email format (RFC 5321)
     * Simplified validation: local@domain
     */
    [[nodiscard]] static bool isEmail(QStringView value) noexcept;

    /**
     * @brief Validate hostname format (RFC 1123)
     */
    [[nodiscard]] static bool isHostname(QStringView value) noexcept;

    /**
     * @brief Validate IPv4 address
     * Format: xxx.xxx.xxx.xxx where xxx is 0-255
     */
    [[nodiscard]] static bool isIpv4(QStringView value) noexcept;

    /**
     * @brief Validate IPv6 address (RFC 4291)
     */
    [[nodiscard]] static bool isIpv6(QStringView value) noexcept;

    /**
     * @brief Validate URI format (RFC 3986)
     */
    [[nodiscard]] static bool isUri(QStringView value) noexcept;

    /**
     * @brief Validate URI reference (RFC 3986)
     * Can be relative or absolute
     */
    [[nodiscard]] static bool isUriReference(QStringView value) noexcept;

    /**
     * @brief Validate URI template (RFC 6570)
     */
    [[nodiscard]] static bool isUriTemplate(QStringView value) noexcept;

    /**
     * @brief Validate UUID format (RFC 4122)
     * Format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
     */
    [[nodiscard]] static bool isUuid(QStringView value) noexcept;

    /**
     * @brief Validate JSON Pointer (RFC 6901)
     */
    [[nodiscard]] static bool isJsonPointer(QStringView value) noexcept;

    /**
     * @brief Validate relative JSON Pointer
     */
    [[nodiscard]] static bool isRelativeJsonPointer(QStringView value) noexcept;

    /**
     * @brief Validate regex format (ECMA-262)
     */
    [[nodiscard]] static bool isRegex(QStringView value) noexcept;

    /**
     * @brief Dispatch to appropriate validator based on format name
     *
     * @param format The format name (e.g., "date-time", "email")
     * @param value The value to validate
     * @return true if valid, false if invalid or format unknown
     */
    [[nodiscard]] static bool validate(QStringView format, QStringView value) noexcept;
};

} // namespace json_query::json_schema::internal
