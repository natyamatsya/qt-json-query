// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include <QtCore/QString>
#include <array>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::utils::detail
{

// Helper macro to define both UTF-8 and UTF-16 string literals
#define DEFINE_ERROR_STRING(str) {str, u##str}

// Structure to hold both UTF-8 and UTF-16 string literals
struct ErrorString
{
    const char*     utf8;
    const char16_t* utf16;
};

// A constexpr map from enum values to error messages
template <typename Enum, size_t N>
class ErrorMap
{
    static_assert(std::is_enum_v<Enum>, "ErrorMap can only be used with enum types");

    using Pair = std::pair<Enum, ErrorString>;
    std::array<Pair, N> entries_;

  public:
    // Constructor that takes an initializer list of enum-message pairs
    constexpr ErrorMap(std::initializer_list<Pair> init)
    {
        if (init.size() != N)
            throw std::invalid_argument("Incorrect number of entries in ErrorMap initializer list");

        size_t i = 0;
        for (const auto& entry : init)
            entries_[i++] = entry;
    }

    // Get error message as string_view
    [[nodiscard]] constexpr std::string_view get_std_sv(Enum e) const noexcept
    {
        for (const auto& [key, value] : entries_)
            if (key == e)
                return value.utf8;
        return "Unknown error";
    }

    // Get error message as QStringView
    [[nodiscard]] constexpr QStringView get_qt_sv(Enum e) const noexcept
    {
        for (const auto& [key, value] : entries_)
            if (key == e)
                return QStringView(value.utf16);
        return QStringView(u"Unknown error");
    }
};

} // namespace json_query::utils::detail
