// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include <QtCore/QRegularExpression>
#include <QtCore/QString>

#ifdef JSON_QUERY_HAS_SRELL
#include <srell.hpp>
#include <string>
#endif

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_schema::internal
{

#ifdef JSON_QUERY_HAS_SRELL

/// ECMAScript-compatible regex using SRELL (when available)
class SchemaRegex
{
  public:
    SchemaRegex() = default;

    /// Compile a pattern string. Returns false if the pattern is invalid.
    [[nodiscard]] bool compile(const QString& pattern) noexcept
    {
        try
        {
            regex_.assign(pattern.toStdU16String(), srell::regex_constants::ECMAScript);
            valid_ = true;
        }
        catch (...)
        {
            valid_ = false;
        }
        return valid_;
    }

    /// Test whether the regex matches anywhere in the string
    [[nodiscard]] bool hasMatch(const QString& str) const
    {
        if (!valid_)
            return false;
        const auto u16{str.toStdU16String()};
        return srell::regex_search(u16, regex_);
    }

    [[nodiscard]] bool isValid() const noexcept { return valid_; }

  private:
    srell::u16regex regex_{};
    bool            valid_{false};
};

#else

/// Fallback regex using QRegularExpression (PCRE)
class SchemaRegex
{
  public:
    SchemaRegex() = default;

    [[nodiscard]] bool compile(const QString& pattern) noexcept
    {
        regex_ = QRegularExpression{pattern};
        regex_.optimize();
        return regex_.isValid();
    }

    [[nodiscard]] bool hasMatch(const QString& str) const { return regex_.match(str).hasMatch(); }

    [[nodiscard]] bool isValid() const noexcept { return regex_.isValid(); }

  private:
    QRegularExpression regex_{};
};

#endif

} // namespace json_query::json_schema::internal
