// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <QString>
#include <QStringView>
#include "json-query/utils/QtStringLiterals.hpp"

namespace json_query::json_pointer
{
/**
 * @brief Escape a JSON Pointer token according to RFC 6901
 *
 * Escapes special characters:
 * - '~' becomes '~0'
 * - '/' becomes '~1'
 *
 * @param token The unescaped token (property name or array index string)
 * @return The escaped token safe for use in a JSON Pointer
 */
[[nodiscard]] inline QString escapeToken(QStringView token) noexcept
{
    using json_query::literals::operator""_qt_s;

    QString out{};
    out.reserve(token.size());
    for (const QChar c : token)
        if (c == u'~')
            out += u"~0"_qt_s;
        else if (c == u'/')
            out += u"~1"_qt_s;
        else
            out += c;
    return out;
}

/**
 * @brief Append a property name token to a JSON Pointer path
 *
 * Properly escapes the token and appends it with a leading slash.
 *
 * @param path The current JSON Pointer path (e.g., "/foo/bar")
 * @param token The property name to append (will be escaped)
 * @return The extended path (e.g., "/foo/bar/baz")
 */
[[nodiscard]] inline QString appendToken(const QString& path, QStringView token) noexcept
{
    return path + u'/' + escapeToken(token);
}

/**
 * @brief Append an array index to a JSON Pointer path
 *
 * @param path The current JSON Pointer path (e.g., "/items")
 * @param index The array index to append
 * @return The extended path (e.g., "/items/0")
 */
[[nodiscard]] inline QString appendIndex(const QString& path, qsizetype index) noexcept
{
    return path + u'/' + QString::number(index);
}

} // namespace json_query::json_pointer
