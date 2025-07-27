#pragma once

// Modern utilities for JSONQuery library
// Formerly in json_query_utils.h; now canonical header uses CamelCase and .hpp suffix.

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <utility>

#include <QString>
#include <QByteArray>
#include <ctre.hpp>

namespace json_query::utils
{
// --- String Conversions ---
[[nodiscard]] inline std::string qstring_to_std_string(const QString& qstr)
{
    const QByteArray byteArray{qstr.toUtf8()};
    return std::string{byteArray.constData(), static_cast<size_t>(byteArray.length())};
}

[[nodiscard]] inline QString std_string_to_qstring(const std::string& str)
{
    return QString::fromUtf8(str.c_str(), static_cast<int>(str.length()));
}

[[nodiscard]] inline QString std_string_view_to_qstring(std::string_view sv)
{
    return QString::fromUtf8(sv.data(), static_cast<int>(sv.size()));
}

// Helper that provides a short-lived std::string_view for a QString.
// The underlying storage is thread_local so the view remains valid for the duration
// of the expression in which it is used (non-reentrant).
[[nodiscard]] inline std::string_view to_sv(const QString& qstr)
{
    thread_local std::string storage;
    storage = qstring_to_std_string(qstr);
    return std::string_view{storage};
}

[[nodiscard]] inline QString to_qstr(std::string_view sv) { return std_string_view_to_qstring(sv); }

} // namespace json_query::utils
