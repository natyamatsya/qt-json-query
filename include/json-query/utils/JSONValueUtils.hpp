// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "JSONQueryError.hpp" // QueryError, ConvertError, toQString(View)

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <expected>
#include <concepts>
#include <limits>
#include <cmath>
#include <utility>

namespace json_query
{

//------------------------------------------------------------------------------
// Targets supported by the conversion core
//------------------------------------------------------------------------------

template <class T>
concept JsonTarget = std::same_as<T, QJsonArray> || std::same_as<T, QJsonObject> || std::same_as<T, QString> ||
                     std::same_as<T, double> || std::same_as<T, int> || std::same_as<T, bool>;
// Note: Returning QStringView would dangle; prefer QString instead.

//------------------------------------------------------------------------------
// Core conversions: QJsonValue -> std::expected<T, QueryError>
//------------------------------------------------------------------------------

namespace detail
{

template <JsonTarget T>
inline std::expected<T, QueryError> as_core(const QJsonValue& v);

// Arrays
template <>
inline std::expected<QJsonArray, QueryError> as_core(const QJsonValue& v)
{
    if (v.isArray())
        return v.toArray();
    return std::unexpected(QueryError{ConvertError::TypeMismatch});
}

// Objects
template <>
inline std::expected<QJsonObject, QueryError> as_core(const QJsonValue& v)
{
    if (v.isObject())
        return v.toObject();
    return std::unexpected(QueryError{ConvertError::TypeMismatch});
}

// Strings
template <>
inline std::expected<QString, QueryError> as_core(const QJsonValue& v)
{
    if (v.isString())
        return v.toString();
    return std::unexpected(QueryError{ConvertError::TypeMismatch});
}

// Booleans
template <>
inline std::expected<bool, QueryError> as_core(const QJsonValue& v)
{
    if (v.isBool())
        return v.toBool();
    return std::unexpected(QueryError{ConvertError::TypeMismatch});
}

// Doubles
template <>
inline std::expected<double, QueryError> as_core(const QJsonValue& v)
{
    if (v.isDouble())
        return v.toDouble();
    return std::unexpected(QueryError{ConvertError::TypeMismatch});
}

// Ints (range + integrality checks)
template <>
inline std::expected<int, QueryError> as_core(const QJsonValue& v)
{
    if (!v.isDouble())
        return std::unexpected(QueryError{ConvertError::TypeMismatch});

    const double d = v.toDouble();

    if (!std::isfinite(d))
        return std::unexpected(QueryError{ConvertError::NumericOutOfRange});

    if (d < static_cast<double>(std::numeric_limits<int>::min()) ||
        d > static_cast<double>(std::numeric_limits<int>::max()))
        return std::unexpected(QueryError{ConvertError::NumericOutOfRange});

    if (std::trunc(d) != d)
        return std::unexpected(QueryError{ConvertError::NumericNotIntegral});

    return static_cast<int>(d);
}

} // namespace detail

//------------------------------------------------------------------------------
// Public helpers
//   - as<T>(QJsonValue)                      -> expected<T, QueryError>
//   - as<T>(expected<QJsonValue, QueryError>) -> expected<T, QueryError>
//   - pipe: expected<QJsonValue, QueryError> | as<T>
//------------------------------------------------------------------------------

template <JsonTarget T>
struct AsFn
{
    // 1) Plain value: domain-agnostic conversion (ConvertError domain)
    [[nodiscard]] constexpr auto operator()(QJsonValue v) const -> std::expected<T, QueryError>
    {
        return detail::as_core<T>(v);
    }

    // 2) Chaining case: preserve unified error (QueryError)
    [[nodiscard]] constexpr auto
    operator()(const std::expected<QJsonValue, QueryError>& r) const -> std::expected<T, QueryError>
    {
        if (!r)
            return std::unexpected(r.error());
        return detail::as_core<T>(*r);
    }
};

// Variable template for ergonomic calls: as<T>(...)
template <JsonTarget T>
inline constexpr AsFn<T> as{};

// Pipeline sugar: result | as<T>
template <class LHS, JsonTarget T>
[[nodiscard]] constexpr auto operator|(LHS&& lhs, const AsFn<T>& f) -> decltype(f(std::forward<LHS>(lhs)))
{
    return f(std::forward<LHS>(lhs));
}

} // namespace json_query
