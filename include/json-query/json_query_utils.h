#pragma once

#include <QString>
#include <QVector>
#include <QPair>
#include <ctre.hpp>

namespace json_query::utils
{
    // Convert QString to std::string_view for CTRE
    inline std::string_view to_sv(const QString &str)
    {
        return std::string_view(str.toUtf8().constData(), str.size());
    }

    // Convert std::string_view back to QString
    inline QString to_qstr(std::string_view sv)
    {
        return QString::fromUtf8(sv.data(), sv.size());
    }

    // Example compile-time regex patterns for JSONPath parsing
    constexpr auto root_pattern = ctll::fixed_string{"^\\$|^@"};
    constexpr auto property_pattern = ctll::fixed_string{"\\.([a-zA-Z0-9_]+|\\*)"};
    constexpr auto quoted_property_pattern = ctll::fixed_string{"\\['([^']*)'\\]"};
    constexpr auto array_index_pattern = ctll::fixed_string{"\\[(\\d+)\\]"};
    constexpr auto array_neg_index_pattern = ctll::fixed_string{"\\[(-\\d+)\\]"};
    constexpr auto array_slice_pattern = ctll::fixed_string{"\\[(\\d*):(-?\\d*)(?::(\\d+))?\\]"};
    constexpr auto array_wildcard_pattern = ctll::fixed_string{"\\[\\*\\]"};
    constexpr auto recursive_descent_pattern = ctll::fixed_string{"\\.\\."};
    constexpr auto filter_pattern = ctll::fixed_string{"\\[\\?\\((.+?)\\)\\]"};

    // For filter expressions
    constexpr auto eq_expr_pattern = ctll::fixed_string{"@\\.(\\w+)\\s*==\\s*['\"](.*)['\"]"};
    constexpr auto num_comp_expr_pattern = ctll::fixed_string{"@\\.(\\w+)\\s*(>|<|>=|<=)\\s*(\\d+(?:\\.\\d+)?)"};

    // For JSONPointer
    constexpr auto escape_sequence_pattern = ctll::fixed_string{"~[01]"};

    // Helper to check if a string matches a CTRE pattern
    template <auto &Pattern>
    bool matches(const QString &str)
    {
        return ctre::match<Pattern>(to_sv(str));
    }

    // Helper to extract the first capture group
    template <auto &Pattern>
    QString capture(const QString &str)
    {
        if (auto m = ctre::match<Pattern>(to_sv(str)))
        {
            if (m.get<1>())
            {
                return to_qstr(m.get<1>().to_view());
            }
        }
        return QString();
    }

    // Helper to extract multiple capture groups into a vector
    template <auto &Pattern>
    QVector<QString> captures(const QString &str)
    {
        QVector<QString> result;
        if (auto m = ctre::match<Pattern>(to_sv(str)))
        {
            // Start from capture group 1 (skipping the entire match)
            for (size_t i = 1; i < m.size(); ++i)
            {
                if (m.get<i>())
                {
                    result.append(to_qstr(m.get<i>().to_view()));
                }
                else
                {
                    result.append(QString());
                }
            }
        }
        return result;
    }

    // Get positions of all matches of a pattern in a string
    template <auto &Pattern>
    QVector<QPair<int, int>> find_all_positions(const QString &str)
    {
        QVector<QPair<int, int>> positions;
        auto sv = to_sv(str);
        auto search_result = ctre::range<Pattern>(sv);

        for (auto match : search_result)
        {
            positions.append(QPair<int, int>(
                match.get<0>().begin() - sv.begin(),
                match.get<0>().end() - sv.begin()));
        }

        return positions;
    }

} // namespace json_query::utils
