#pragma once

// Modern utilities for JSONQuery library
// Formerly in json_query_utils.h; now canonical header uses CamelCase and .hpp suffix.

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <utility>
#include <stdexcept>

#include <QString>
#include <QByteArray>

#include <ctre.hpp>

namespace json_query::utils
{
// --- String Conversions ---
[[nodiscard]] inline std::string qstring_to_std_string(const QString &qstr)
{
    const QByteArray byteArray{qstr.toUtf8()};
    return std::string{byteArray.constData(), static_cast<size_t>(byteArray.length())};
}

[[nodiscard]] inline QString std_string_to_qstring(const std::string &str)
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
[[nodiscard]] inline std::string_view to_sv(const QString &qstr)
{
    thread_local std::string storage;
    storage = qstring_to_std_string(qstr);
    return std::string_view{storage};
}

[[nodiscard]] inline QString to_qstr(std::string_view sv)
{
    return std_string_view_to_qstring(sv);
}

// --- Common CTRE patterns used across JSONPath / JSONPointer ---
constexpr auto root_pattern                 = ctll::fixed_string{"^\\$|^@"};
constexpr auto property_pattern             = ctll::fixed_string{"\\.([a-zA-Z0-9_]+|\\*)"};
constexpr auto quoted_property_pattern      = ctll::fixed_string{"\\['([^']*)'\\]"};
constexpr auto array_index_pattern          = ctll::fixed_string{"\\[(\\d+)\\]"};
constexpr auto array_neg_index_pattern      = ctll::fixed_string{"\\[(-\\d+)\\]"};
constexpr auto array_slice_pattern          = ctll::fixed_string{"\\[(\\d*):(-?\\d*)(?::(\\d+))?\\]"};
constexpr auto array_wildcard_pattern       = ctll::fixed_string{"\\[\\*\\]"};
constexpr auto recursive_descent_pattern    = ctll::fixed_string{"\\.\\."};
constexpr auto filter_pattern               = ctll::fixed_string{"\\[\\?\\((.+?)\\)\\]"};
constexpr auto eq_expr_pattern              = ctll::fixed_string{"@\\.(\\w+)\\s*==\\s*['\"](.*)['\"]"};
constexpr auto num_comp_expr_pattern        = ctll::fixed_string{"@\\.(\\w+)\\s*(>|<|>=|<=)\\s*(\\d+(?:\\.\\d+)?)"};
constexpr auto escape_sequence_pattern      = ctll::fixed_string{"~[01]"};

// --- Helper wrappers around CTRE for convenience with QString ---

template <auto &Pattern>
[[nodiscard]] inline bool matches(std::string_view sv) noexcept { return ctre::match<Pattern>(sv); }

template <auto &Pattern>
[[nodiscard]] inline bool matches(const QString &q) noexcept { return matches<Pattern>(to_sv(q)); }

// capture, captures, find_all_positions as in original header (updated template keywords)

template <auto &Pattern>
[[nodiscard]] inline std::optional<std::string> capture(std::string_view sv)
{
    if (const auto m = ctre::match<Pattern>(sv))
    {
        if (const auto group1 = m.template get<1>())
            return std::string{group1.data(), group1.length()};
    }
    return std::nullopt;
}

template <auto &Pattern>
[[nodiscard]] inline std::vector<std::string> captures(std::string_view sv)
{
    std::vector<std::string> out;
    if (const auto m = ctre::match<Pattern>(sv))
    {
        for (size_t i = 1; i < m.size(); ++i)
        {
            if (const auto g = m.template get<i>())
                out.emplace_back(g.data(), g.length());
        }
    }
    return out;
}

template <auto &Pattern>
[[nodiscard]] inline std::vector<std::pair<size_t,size_t>> find_all_positions(std::string_view sv)
{
    std::vector<std::pair<size_t,size_t>> res;
    for (const auto match : ctre::search_all<Pattern>(sv))
    {
        const auto full = match.template get<0>();
        res.emplace_back(static_cast<size_t>(full.begin()-sv.begin()), static_cast<size_t>(full.end()-sv.begin()));
    }
    return res;
}

template <auto &Pattern>
[[nodiscard]] inline std::vector<std::pair<size_t,size_t>> find_all_positions(const QString &q)
{
    return find_all_positions<Pattern>(to_sv(q));
}

template <auto &Pattern>
[[nodiscard]] inline std::optional<std::string> capture(const QString &q)
{
    return capture<Pattern>(to_sv(q));
}

template <auto &Pattern>
[[nodiscard]] inline std::vector<std::string> captures(const QString &q)
{
    return captures<Pattern>(to_sv(q));
}

} // namespace json_query::utils

// Short alias retained for existing code
namespace json_utils = json_query::utils;



