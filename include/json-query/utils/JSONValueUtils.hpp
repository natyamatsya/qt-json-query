// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include "JSONError.hpp"

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <expected>
#include <concepts>
#include <limits>
#include <cmath>
#include <utility>
#include <type_traits>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS
{

//------------------------------------------------------------------------------
// Kind of a runtime value (JsonKind + kind_name live in JSONError.hpp so
// formatted_message() can decode Convert-error diagnostics)
//------------------------------------------------------------------------------

inline JsonKind kind_of(const QJsonValue& v) noexcept
{
    switch (v.type())
    {
    case QJsonValue::Null:
        return JsonKind::Null;
    case QJsonValue::Object:
        return JsonKind::Object;
    case QJsonValue::Array:
        return JsonKind::Array;
    case QJsonValue::String:
        return JsonKind::String;
    case QJsonValue::Double:
        return JsonKind::Number;
    case QJsonValue::Bool:
        return JsonKind::Bool;
    default:
        return JsonKind::Undefined;
    }
}

//------------------------------------------------------------------------------
// Targets supported by the conversion core
//------------------------------------------------------------------------------

template <class T>
concept JsonTarget = std::same_as<T, QJsonArray> || std::same_as<T, QJsonObject> || std::same_as<T, QString> ||
                     std::same_as<T, double> || std::same_as<T, int> || std::same_as<T, qint64> ||
                     std::same_as<T, bool>;

//------------------------------------------------------------------------------
// Core conversions: QJsonValue -> std::expected<T, Error>. Conversion errors
// use the unified Error type directly (ErrorDomain::Convert), with the
// expected/actual JsonKind pair packed into Error::detail (pack_kinds) so
// formatted_message() renders "... (expected array, got string)".
//------------------------------------------------------------------------------

namespace detail
{

[[nodiscard]] inline Error typeMismatch(JsonKind expected, const QJsonValue& v) noexcept
{
    return Error{ConvertError::TypeMismatch, pack_kinds(expected, kind_of(v))};
}

template <JsonTarget T>
inline std::expected<T, Error> as_core(const QJsonValue& v);

// Arrays
template <>
inline std::expected<QJsonArray, Error> as_core(const QJsonValue& v)
{
    if (v.isArray())
        return v.toArray();
    return std::unexpected(typeMismatch(JsonKind::Array, v));
}

// Objects
template <>
inline std::expected<QJsonObject, Error> as_core(const QJsonValue& v)
{
    if (v.isObject())
        return v.toObject();
    return std::unexpected(typeMismatch(JsonKind::Object, v));
}

// Strings
template <>
inline std::expected<QString, Error> as_core(const QJsonValue& v)
{
    if (v.isString())
        return v.toString();
    return std::unexpected(typeMismatch(JsonKind::String, v));
}

// Booleans
template <>
inline std::expected<bool, Error> as_core(const QJsonValue& v)
{
    if (v.isBool())
        return v.toBool();
    return std::unexpected(typeMismatch(JsonKind::Bool, v));
}

// Doubles
template <>
inline std::expected<double, Error> as_core(const QJsonValue& v)
{
    if (v.isDouble())
        return v.toDouble();
    return std::unexpected(typeMismatch(JsonKind::Number, v));
}

// 64-bit integers (Qt's native JSON integer width). QJsonValue::toInteger is
// exact for integer-backed values (no round-trip through double, so the full
// qint64 range converts losslessly); doubles must be integral and in range.
template <>
inline std::expected<qint64, Error> as_core(const QJsonValue& v)
{
    if (!v.isDouble())
        return std::unexpected(typeMismatch(JsonKind::Number, v));

    // toInteger returns its default when the value is not representable as a
    // whole qint64 — two calls with different defaults disambiguate
    const qint64 a = v.toInteger(0);
    const qint64 b = v.toInteger(1);
    if (a == b)
        return a;

    constexpr auto numberPair{pack_kinds(JsonKind::Number, JsonKind::Number)};
    const double   d = v.toDouble();
    if (std::isfinite(d) && std::trunc(d) != d)
        return std::unexpected(Error{ConvertError::NumericNotIntegral, numberPair});
    return std::unexpected(Error{ConvertError::NumericOutOfRange, numberPair});
}

// Ints (range + integrality checks)
template <>
inline std::expected<int, Error> as_core(const QJsonValue& v)
{
    constexpr auto numberPair{pack_kinds(JsonKind::Number, JsonKind::Number)};

    if (!v.isDouble())
        return std::unexpected(typeMismatch(JsonKind::Number, v));

    const double d = v.toDouble();
    if (!std::isfinite(d))
        return std::unexpected(Error{ConvertError::NumericOutOfRange, numberPair});

    if (d < static_cast<double>(std::numeric_limits<int>::min()) ||
        d > static_cast<double>(std::numeric_limits<int>::max()))
        return std::unexpected(Error{ConvertError::NumericOutOfRange, numberPair});

    if (std::trunc(d) != d)
        return std::unexpected(Error{ConvertError::NumericNotIntegral, numberPair});

    return static_cast<int>(d);
}

} // namespace detail

//------------------------------------------------------------------------------
// Public helpers
//   - as<T>(QJsonValue)               -> expected<T, Error>
//   - as<T>(expected<QJsonValue,Error>) -> expected<T, Error>
//   - pipe: QJsonValue (or QJsonValueRef) | as<T>
//   NOTE: The second overload is templated so it is NOT viable via implicit
//         conversions from QJsonValue/Ref → expected<...>, avoiding ambiguity.
//------------------------------------------------------------------------------

template <JsonTarget T>
struct AsFn
{
    // 1) Plain value (conversion errors are unified Errors directly)
    [[nodiscard]] auto operator()(QJsonValue v) const -> std::expected<T, Error> { return detail::as_core<T>(v); }

    // 2) Chaining case: preserve unified error (Error)
    //    Templated so it doesn't participate in overload resolution
    //    for non-expected arguments.
    template <class E>
        requires std::same_as<std::remove_cv_t<E>, Error>
    [[nodiscard]] auto operator()(const std::expected<QJsonValue, E>& r) const -> std::expected<T, E>
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
// Generic overload for everything except QJsonValueRef.
template <class LHS, JsonTarget T>
    requires(!std::is_same_v<std::remove_cvref_t<LHS>, QJsonValueRef>)
[[nodiscard]] auto operator|(LHS&& lhs, const AsFn<T>& f) -> decltype(f(std::forward<LHS>(lhs)))
{
    return f(std::forward<LHS>(lhs));
}

// Special-case overload to handle QJsonValueRef (produced by Qt's operator[])
// without colliding with Qt's many operator| flag overloads.
template <JsonTarget T>
[[nodiscard]] inline auto operator|(QJsonValueRef lhs, const AsFn<T>& f) -> std::expected<T, Error>
{
    return f(QJsonValue(lhs));
}

//------------------------------------------------------------------------------
// Terminal default-value adapter: as_or<T>(fallback)
//   The end of a monadic chain — converts like as<T> but yields T directly,
//   falling back on ANY failure along the way (evaluation error, missing
//   value, or conversion failure):
//
//     const auto name{"/data/name"_jptr.evaluate(doc) | as_or<QString>()};
//     const auto port{cfg.evaluate(doc) | as_or<int>(8080)};
//------------------------------------------------------------------------------

template <JsonTarget T>
struct AsOrFn
{
    T fallback;

    // 1) Plain value: convert or fall back
    [[nodiscard]] T operator()(const QJsonValue& v) const
    {
        auto base{detail::as_core<T>(v)};
        return base ? *std::move(base) : fallback;
    }

    // 2) Chaining case: an errored expected falls back too. Templated so it
    //    is not viable via implicit conversions (mirrors AsFn).
    template <class E>
        requires std::same_as<std::remove_cv_t<E>, Error>
    [[nodiscard]] T operator()(const std::expected<QJsonValue, E>& r) const
    {
        return r ? (*this)(*r) : fallback;
    }
};

/// Make the terminal adapter; the default fallback is a value-initialized T
/// (empty QString, 0, empty array, ...).
template <JsonTarget T>
[[nodiscard]] AsOrFn<T> as_or(T fallback = T{})
{
    return AsOrFn<T>{std::move(fallback)};
}

// Pipeline sugar: result | as_or<T>(...) — expression-SFINAE like the AsFn
// overload, so incompatible left-hand sides fail overload resolution cleanly
template <class LHS, JsonTarget T>
    requires(!std::is_same_v<std::remove_cvref_t<LHS>, QJsonValueRef>)
[[nodiscard]] auto operator|(LHS&& lhs, const AsOrFn<T>& f) -> decltype(f(std::forward<LHS>(lhs)))
{
    return f(std::forward<LHS>(lhs));
}

// QJsonValueRef special case (see the AsFn overload above)
template <JsonTarget T>
[[nodiscard]] inline auto operator|(QJsonValueRef lhs, const AsOrFn<T>& f) -> T
{
    return f(QJsonValue(lhs));
}

} // namespace json_query
