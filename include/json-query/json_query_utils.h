#pragma once

// Standard Library Includes
#include <string>
#include <string_view>
#include <vector>
#include <optional>  // C++17, but essential here
#include <utility>   // For std::pair
#include <stdexcept> // For potential future exceptions

// Qt Includes (Only for necessary conversions)
#include <QString>
#include <QByteArray>

// CTRE Include
#include <ctre.hpp>

namespace json_query::utils
{
    // --- String Conversions (UTF-8) ---
    // These ensure safe handling between Qt's UTF-16 and std::string/view's typical UTF-8

    // Convert QString to std::string (UTF-8). Returns a new string object.
    [[nodiscard]] inline std::string qstring_to_std_string(const QString &qstr)
    {
        const QByteArray byteArray{qstr.toUtf8()};
        // Use brace-init for construction
        return std::string{byteArray.constData(), static_cast<size_t>(byteArray.length())};
    }

    // Convert std::string (assumed UTF-8) to QString
    [[nodiscard]] inline QString std_string_to_qstring(const std::string &str)
    {
        // Ensure length parameter type matches expected type
        return QString::fromUtf8(str.c_str(), static_cast<int>(str.length()));
    }

    // Convert std::string_view (assumed UTF-8) to QString
    [[nodiscard]] inline QString std_string_view_to_qstring(const std::string_view sv)
    {
        // Ensure length parameter type matches expected type
        return QString::fromUtf8(sv.data(), static_cast<int>(sv.size()));
    }

    // --- CTRE Patterns (JSONPath examples & JSON Pointer escape) ---
    // (Keep your existing patterns as they are compile-time constants)
    constexpr auto root_pattern = ctll::fixed_string{"^\\$|^@"};
    constexpr auto property_pattern = ctll::fixed_string{"\\.([a-zA-Z0-9_]+|\\*)"};
    constexpr auto quoted_property_pattern = ctll::fixed_string{"\\['([^']*)'\\]"};
    constexpr auto array_index_pattern = ctll::fixed_string{"\\[(\\d+)\\]"};
    constexpr auto array_neg_index_pattern = ctll::fixed_string{"\\[(-\\d+)\\]"};
    constexpr auto array_slice_pattern = ctll::fixed_string{"\\[(\\d*):(-?\\d*)(?::(\\d+))?\\]"};
    constexpr auto array_wildcard_pattern = ctll::fixed_string{"\\[\\*\\]"};
    constexpr auto recursive_descent_pattern = ctll::fixed_string{"\\.\\."};
    constexpr auto filter_pattern = ctll::fixed_string{"\\[\\?\\((.+?)\\)\\]"};
    constexpr auto eq_expr_pattern = ctll::fixed_string{"@\\.(\\w+)\\s*==\\s*['\"](.*)['\"]"};
    constexpr auto num_comp_expr_pattern = ctll::fixed_string{"@\\.(\\w+)\\s*(>|<|>=|<=)\\s*(\\d+(?:\\.\\d+)?)"};
    constexpr auto escape_sequence_pattern = ctll::fixed_string{"~[01]"}; // For JSON Pointer

    // --- CTRE Helper Functions (Revised for Safety and std types) ---
    // IMPORTANT: The caller MUST ensure the lifetime of the std::string_view 'sv'
    //            argument is valid for the duration of the function call.
    //            Typically, create a std::string first (e.g., using qstring_to_std_string)
    //            and then pass a view of that string to these helpers.

    // Check if a string_view *completely* matches a CTRE pattern.
    template <auto &Pattern>
    [[nodiscard]] inline bool matches(const std::string_view sv) noexcept
    { // noexcept if ctre::match is
        // ctre::match checks if the *entire* input matches the pattern.
        return ctre::match<Pattern>(sv);
    }

    // Extract the content of the first capture group (index 1) as std::string.
    // Returns std::nullopt if the pattern doesn't match or capture group 1 is invalid.
    template <auto &Pattern>
    [[nodiscard]] inline std::optional<std::string> capture(const std::string_view sv)
    {
        // Use const auto with brace-initialization
        if (const auto m{ctre::match<Pattern>(sv)})
        {
            // Use '.template get<N>()' for dependent template types
            if (const auto group1{m.template get()})
            {   // Check capture group 1 specifically
                // Create std::string using brace-init from string_view data/length
                return std::string{group1.data(), group1.length()};
            }
            // Pattern matched, but capture group 1 was not valid/present
            // Fall through to return std::nullopt
        }
        // Pattern did not match the input view
        return std::nullopt;
    }

    // Extract all *successful* capture groups (indices >= 1) into a vector of strings.
    // Skips groups that didn't participate in the match.
    template <auto &Pattern>
    [[nodiscard]] inline std::vector<std::string> captures(const std::string_view sv)
    {
        std::vector<std::string> result{}; // Brace-init empty vector
        if (const auto m{ctre::match<Pattern>(sv)})
        {
            // Optional: Reserve space (heuristic)
            // constexpr size_t num_groups = ctre::details::get_capture_count<...>(); // Hard to get reliably
            // result.reserve(m.size() > 0 ? m.size() - 1 : 0);

            // Iterate capture groups starting from index 1 (index 0 is the full match)
            for (size_t i{1}; i < m.size(); ++i)
            {
                // Use .template get<i>() for dependent types
                if (const auto group{m.template get<i>()})
                {
                    // Construct string directly in the vector for efficiency
                    result.emplace_back(group.data(), group.length());
                }
                // Implicitly skips groups where m.template get<i>() evaluates to false (empty optional)
            }
        }
        return result; // Return the vector (potentially empty)
    }

    // Find the start and end positions (indices relative to 'sv') of all
    // non-overlapping matches of 'Pattern' within the string_view 'sv'.
    template <auto &Pattern>
    [[nodiscard]] inline std::vector<std::pair<size_t, size_t>> find_all_positions(const std::string_view sv)
    {
        std::vector<std::pair<size_t, size_t>> positions{}; // Brace-init empty vector

        // ctre::range iterates through all non-overlapping matches in the view
        for (const auto match : ctre::range<Pattern>(sv))
        {
            // Get the string_view for the entire match (group 0)
            // Use const auto with brace-initialization
            const auto full_match_view{match.template get()}; // Group 0 is the full match

            // Calculate positions relative to the start of the input view 'sv'
            // Use static_cast for pointer difference to size_t conversion.
            const size_t start_pos{static_cast<size_t>(full_match_view.begin() - sv.begin())};
            const size_t end_pos{static_cast<size_t>(full_match_view.end() - sv.begin())};

            // Use emplace_back for efficiency
            positions.emplace_back(start_pos, end_pos);
        }

        return positions; // Return vector of position pairs
    }

} // namespace json_query::utils
