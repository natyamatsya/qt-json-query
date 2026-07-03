// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

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
 * - _qt_s  : Creates QString from UTF-16 literal
 * - _qt_sv : Creates QStringView from UTF-16 literal (zero-copy)
 * - _qt_l1 : Creates QLatin1String from ASCII literal (zero-copy)
 *
 * Usage:
 *   using json_query::literals::operator""_qt_s;
 *   using json_query::literals::operator""_qt_sv;
 *   using json_query::literals::operator""_qt_l1;
 *
 *   auto str  = u"hello"_qt_s;   // QString
 *   auto view = u"hello"_qt_sv;  // QStringView
 *   auto l1   = "hello"_qt_l1;   // QLatin1String
 */
#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::literals
{

/// Creates QString from UTF-16 string literal
inline QString operator""_qt_s(const char16_t* str, std::size_t size)
{
    return Qt::StringLiterals::operator""_s(str, size);
}

/// Creates QStringView from UTF-16 string literal (zero-copy, non-owning)
constexpr QStringView operator""_qt_sv(const char16_t* str, std::size_t size) noexcept
{
    return QStringView(str, static_cast<qsizetype>(size));
}

/// Creates QLatin1String from ASCII string literal (zero-copy, non-owning)
constexpr QLatin1String operator""_qt_l1(const char* str, std::size_t size) noexcept
{
    return QLatin1String(str, static_cast<qsizetype>(size));
}

} // namespace json_query::literals
