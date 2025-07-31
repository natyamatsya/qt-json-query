// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include "JSONQueryError.hpp"

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

namespace json_query
{

//------------------------------------------------------------------------------
// Domain-agnostic kinds (for diagnostics inside this file only)
//------------------------------------------------------------------------------

enum class JsonKind
{
    Null,
    Object,
    Array,
    String,
    Number,
    Bool,
    Undefined
};

inline constexpr JsonKind kind_of(const QJsonValue& v) noexcept
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

inline constexpr std::string_view kind_name(JsonKind k) noexcept
{
    switch (k)
    {
    case JsonKind::Null:
        return "null";
    case JsonKind::Object:
        return "object";
    case JsonKind::Array:
        return "array";
    case JsonKind::String:
        return "string";
    case JsonKind::Number:
        return "number";
    case JsonKind::Bool:
        return "bool";
    case JsonKind::Undefined:
        return "undefined";
    }
    return "unknown";
}

// For local conversion core we use a small internal error,
// then map it to the unified QueryError on the API boundary.
enum class ConvErrorCode
{
    TypeMismatch,
    OutOfRange,
    NotIntegral
};

struct ConvError
{
    ConvErrorCode code;
    JsonKind      expected;
    JsonKind      actual;
};

inline QString errorMessage(const ConvError& e)
{
    using enum ConvErrorCode;
    switch (e.code)
    {
    case TypeMismatch:
        return QStringLiteral("Type mismatch: expected %1, got %2")
            .arg(QString::fromUtf8(kind_name(e.expected).data()))
            .arg(QString::fromUtf8(kind_name(e.actual).data()));
    case OutOfRange:
        return QStringLiteral("Numeric conversion out of range");
    case NotIntegral:
        return QStringLiteral("Expected an integral number (no fractional part)");
    }
    return QStringLiteral("Conversion error");
}

// Map ConvError -> unified QueryError (Convert domain)
inline QueryError mapConvError(const ConvError& e) noexcept
{
    using enum ConvErrorCode;
    switch (e.code)
    {
    case TypeMismatch:
        return QueryError{ConvertError::TypeMismatch};
    case OutOfRange:
        return QueryError{ConvertError::NumericOutOfRange};
    case NotIntegral:
        return QueryError{ConvertError::NumericNotIntegral};
    }
    // Fallback – shouldn't happen.
    return QueryError{ConvertError::TypeMismatch};
}

//------------------------------------------------------------------------------
// Targets supported by the conversion core
//------------------------------------------------------------------------------

template <class T>
concept JsonTarget = std::same_as<T, QJsonArray> || std::same_as<T, QJsonObject> || std::same_as<T, QString> ||
                     std::same_as<T, double> || std::same_as<T, int> || std::same_as<T, bool>;

//------------------------------------------------------------------------------
// Core conversions: QJsonValue -> std::expected<T, ConvError>
//------------------------------------------------------------------------------

namespace detail
{

template <JsonTarget T>
inline std::expected<T, ConvError> as_core(const QJsonValue& v);

// Arrays
template <>
inline std::expected<QJsonArray, ConvError> as_core(const QJsonValue& v)
{
    if (v.isArray())
        return v.toArray();
    return std::unexpected(ConvError{ConvErrorCode::TypeMismatch, JsonKind::Array, kind_of(v)});
}

// Objects
template <>
inline std::expected<QJsonObject, ConvError> as_core(const QJsonValue& v)
{
    if (v.isObject())
        return v.toObject();
    return std::unexpected(ConvError{ConvErrorCode::TypeMismatch, JsonKind::Object, kind_of(v)});
}

// Strings
template <>
inline std::expected<QString, ConvError> as_core(const QJsonValue& v)
{
    if (v.isString())
        return v.toString();
    return std::unexpected(ConvError{ConvErrorCode::TypeMismatch, JsonKind::String, kind_of(v)});
}

// Booleans
template <>
inline std::expected<bool, ConvError> as_core(const QJsonValue& v)
{
    if (v.isBool())
        return v.toBool();
    return std::unexpected(ConvError{ConvErrorCode::TypeMismatch, JsonKind::Bool, kind_of(v)});
}

// Doubles
template <>
inline std::expected<double, ConvError> as_core(const QJsonValue& v)
{
    if (v.isDouble())
        return v.toDouble();
    return std::unexpected(ConvError{ConvErrorCode::TypeMismatch, JsonKind::Number, kind_of(v)});
}

// Ints (range + integrality checks)
template <>
inline std::expected<int, ConvError> as_core(const QJsonValue& v)
{
    if (!v.isDouble())
        return std::unexpected(ConvError{ConvErrorCode::TypeMismatch, JsonKind::Number, kind_of(v)});

    const double d = v.toDouble();
    if (!std::isfinite(d))
        return std::unexpected(ConvError{ConvErrorCode::OutOfRange, JsonKind::Number, JsonKind::Number});

    if (d < static_cast<double>(std::numeric_limits<int>::min()) ||
        d > static_cast<double>(std::numeric_limits<int>::max()))
        return std::unexpected(ConvError{ConvErrorCode::OutOfRange, JsonKind::Number, JsonKind::Number});

    if (std::trunc(d) != d)
        return std::unexpected(ConvError{ConvErrorCode::NotIntegral, JsonKind::Number, JsonKind::Number});

    return static_cast<int>(d);
}

} // namespace detail

//------------------------------------------------------------------------------
// Public helpers
//   - as<T>(QJsonValue)               -> expected<T, QueryError>
//   - as<T>(expected<QJsonValue,QueryError>) -> expected<T, QueryError>
//   - pipe: QJsonValue (or QJsonValueRef) | as<T>
//   NOTE: The second overload is templated so it is NOT viable via implicit
//         conversions from QJsonValue/Ref → expected<...>, avoiding ambiguity.
//------------------------------------------------------------------------------

template <JsonTarget T>
struct AsFn
{
    // 1) Plain value: domain-agnostic conversion → mapped to QueryError
    [[nodiscard]] constexpr auto operator()(QJsonValue v) const -> std::expected<T, QueryError>
    {
        auto base = detail::as_core<T>(v);         // expected<T, ConvError>
        return base.transform_error(mapConvError); // expected<T, QueryError>
    }

    // 2) Chaining case: preserve unified error (QueryError)
    //    Templated so it doesn't participate in overload resolution
    //    for non-expected arguments.
    template <class E>
        requires std::same_as<std::remove_cv_t<E>, QueryError>
    [[nodiscard]] constexpr auto operator()(const std::expected<QJsonValue, E>& r) const -> std::expected<T, E>
    {
        if (!r)
            return std::unexpected(r.error());

        auto base = detail::as_core<T>(*r);        // expected<T, ConvError>
        return base.transform_error(mapConvError); // expected<T, QueryError>
    }
};

// Variable template for ergonomic calls: as<T>(...)
template <JsonTarget T>
inline constexpr AsFn<T> as{};

// Pipeline sugar: result | as<T>
// Generic overload for everything except QJsonValueRef.
template <class LHS, JsonTarget T>
    requires(!std::is_same_v<std::remove_cvref_t<LHS>, QJsonValueRef>)
[[nodiscard]] constexpr auto operator|(LHS&& lhs, const AsFn<T>& f) -> decltype(f(std::forward<LHS>(lhs)))
{
    return f(std::forward<LHS>(lhs));
}

// Special-case overload to handle QJsonValueRef (produced by Qt's operator[])
// without colliding with Qt's many operator| flag overloads.
template <JsonTarget T>
[[nodiscard]] inline auto operator|(QJsonValueRef lhs, const AsFn<T>& f) -> std::expected<T, QueryError>
{
    return f(QJsonValue(lhs));
}

} // namespace json_query
