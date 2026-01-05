// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <QString>
#include <QStringView>
#include <QLatin1String>

/**
 * @brief Helper namespace for Qt string literals
 *
 * Qt 6.8 deprecated u"..."_qs in favor of Qt::StringLiterals::_s, but _s
 * conflicts with C++23 std::string literal operator""s. This namespace
 * provides clean aliases that avoid namespace pollution in headers.
 *
 * Available suffixes:
 * - _qt_s   : Creates QString (wraps Qt::StringLiterals::_s)
 * - _qt_sv  : Creates QStringView (wraps Qt::StringLiterals::_sv)
 * - _qt_l1  : Creates QLatin1String (wraps Qt::StringLiterals::_L1)
 *
 * Usage:
 *   using json_query::literals::operator""_qt_s;
 *   using json_query::literals::operator""_qt_sv;
 *   using json_query::literals::operator""_qt_l1;
 *
 *   auto str = u"hello"_qt_s;   // QString
 *   auto view = u"hello"_qt_sv; // QStringView
 *   auto l1 = "hello"_qt_l1;    // QLatin1String
 */
namespace json_query::literals
{

/// Creates QString - alias for Qt::StringLiterals::operator""_s
inline QString operator""_qt_s(const char16_t* str, std::size_t size)
{
    return Qt::StringLiterals::operator""_s(str, size);
}

/// Creates QStringView - alias for Qt::StringLiterals::operator""_sv
inline QStringView operator""_qt_sv(const char16_t* str, std::size_t size) noexcept
{
    return Qt::StringLiterals::operator""_sv(str, size);
}

/// Creates QLatin1String - alias for Qt::StringLiterals::operator""_L1
inline QLatin1String operator""_qt_l1(const char* str, std::size_t size) noexcept
{
    return Qt::StringLiterals::operator""_L1(str, size);
}

} // namespace json_query::literals
