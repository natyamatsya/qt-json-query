// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

#include <expected>
#include <concepts>
#include <limits>
#include <cmath>
#include <utility>

#include "../json-path/JSONPathEvalError.hpp"        // json_query::json_path::EvalError, to_string(...)
#include "../json-pointer/JSONPointerEvaluation.hpp" // json_query::json_pointer::detail::EvalError, to_string(...)

namespace json_query
{

//------------------------------------------------------------------------------
// Domain-agnostic kinds & conversion error
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

//------------------------------------------------------------------------------
// Targets supported by the conversion core
//------------------------------------------------------------------------------

template <class T>
concept JsonTarget = std::same_as<T, QJsonArray> || std::same_as<T, QJsonObject> || std::same_as<T, QString> ||
                     std::same_as<T, double> || std::same_as<T, int> || std::same_as<T, bool>;
// Note: Returning QStringView would dangle; prefer QString instead.

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
    {
        return std::unexpected(ConvError{ConvErrorCode::OutOfRange, JsonKind::Number, JsonKind::Number});
    }
    if (std::trunc(d) != d)
        return std::unexpected(ConvError{ConvErrorCode::NotIntegral, JsonKind::Number, JsonKind::Number});
    return static_cast<int>(d);
}

} // namespace detail

//------------------------------------------------------------------------------
// Error adapters: ConvError -> domain error
// NOTE: JSONPath/JSON Pointer enums only distinguish object vs array mismatches,
//       plus bounds/slice issues. For primitive mismatches and numeric conversion
//       errors, we conservatively map to TypeMismatchArray (tunable).
//------------------------------------------------------------------------------

template <class E>
struct ErrorAdapter; // intentionally undefined by default

// JSONPath mapping
template <>
struct ErrorAdapter<json_query::json_path::EvalError>
{
    static json_query::json_path::EvalError from(const ConvError& e)
    {
        using EP = json_query::json_path::EvalError;
        using enum ConvErrorCode;
        if (e.code == TypeMismatch)
        {
            if (e.expected == JsonKind::Object)
                return EP::TypeMismatchObject;
            if (e.expected == JsonKind::Array)
                return EP::TypeMismatchArray;
            // Primitive kind mismatch: no exact code -> treat as general mismatch.
            return EP::TypeMismatchArray;
        }
        // Numeric errors: no dedicated codes in JSONPath::EvalError -> pick closest generic.
        return EP::TypeMismatchArray;
    }
};

// JSON Pointer mapping
template <>
struct ErrorAdapter<json_query::json_pointer::detail::EvalError>
{
    static json_query::json_pointer::detail::EvalError from(const ConvError& e)
    {
        using EP = json_query::json_pointer::detail::EvalError;
        using enum ConvErrorCode;
        if (e.code == TypeMismatch)
        {
            if (e.expected == JsonKind::Object)
                return EP::TypeMismatchObject;
            if (e.expected == JsonKind::Array)
                return EP::TypeMismatchArray;
            // Primitive kind mismatch: no exact code -> treat as general mismatch.
            return EP::TypeMismatchArray;
        }
        // Numeric errors: no dedicated codes in Pointer::EvalError -> pick closest generic.
        return EP::TypeMismatchArray;
    }
};

//------------------------------------------------------------------------------
// Function-object variable template: as<T>
//   - QJsonValue -> expected<T, ConvError>
//   - expected<QJsonValue, E> -> expected<T, E>  (E auto-deduced)
//------------------------------------------------------------------------------

template <JsonTarget T>
struct AsFn
{
    // Chaining case: preserve domain error E
    template <class E>
    [[nodiscard]] constexpr auto operator()(const std::expected<QJsonValue, E>& r) const -> std::expected<T, E>
    {
        if (!r)
            return std::unexpected(r.error());
        auto base = detail::as_core<T>(*r);
        return base.transform_error(ErrorAdapter<E>::from);
    }

    // Plain value: domain-agnostic conversion
    [[nodiscard]] constexpr auto operator()(const QJsonValue& v) const -> std::expected<T, ConvError>
    {
        return detail::as_core<T>(v);
    }
};

// Variable template for ergonomic calls: as<T>(...)
template <JsonTarget T>
inline constexpr AsFn<T> as{};

// Optional pipeline sugar: result | as<T>
template <class LHS, JsonTarget T>
[[nodiscard]] constexpr auto operator|(LHS&& lhs, const AsFn<T>& f) -> decltype(f(std::forward<LHS>(lhs)))
{
    return f(std::forward<LHS>(lhs));
}

} // namespace json_query

//------------------------------------------------------------------------------
// Extension methods (.as<T>()) using explicit object parameter (C++23)
// Enables: jsonPath.evaluate(doc).as<QJsonArray>()
//------------------------------------------------------------------------------

namespace json_query::json_path
{

#if defined(__cpp_explicit_this_parameter) && __cpp_explicit_this_parameter >= 202110

template <json_query::JsonTarget T>
[[nodiscard]] inline auto as(this std::expected<QJsonValue, EvalError>& self) -> std::expected<T, EvalError>
{
    if (!self)
        return std::unexpected(self.error());
    auto base = json_query::detail::as_core<T>(*self);
    return base.transform_error(json_query::ErrorAdapter<EvalError>::from);
}

template <json_query::JsonTarget T>
[[nodiscard]] inline auto as(this const std::expected<QJsonValue, EvalError>& self) -> std::expected<T, EvalError>
{
    if (!self)
        return std::unexpected(self.error());
    auto base = json_query::detail::as_core<T>(*self);
    return base.transform_error(json_query::ErrorAdapter<EvalError>::from);
}

template <json_query::JsonTarget T>
[[nodiscard]] inline auto as(this std::expected<QJsonValue, EvalError>&& self) -> std::expected<T, EvalError>
{
    if (!self)
        return std::unexpected(std::move(self).error());
    auto base = json_query::detail::as_core<T>(*self);
    return base.transform_error(json_query::ErrorAdapter<EvalError>::from);
}

#else
// Fallback for compilers without explicit-this: ADL free function
template <json_query::JsonTarget T>
[[nodiscard]] inline auto as(const std::expected<QJsonValue, EvalError>& self) -> std::expected<T, EvalError>
{
    if (!self)
        return std::unexpected(self.error());
    auto base = json_query::detail::as_core<T>(*self);
    return base.transform_error(json_query::ErrorAdapter<EvalError>::from);
}
#endif

} // namespace json_query::json_path

namespace json_query::json_pointer::detail
{

#if defined(__cpp_explicit_this_parameter) && __cpp_explicit_this_parameter >= 202110

template <json_query::JsonTarget T>
[[nodiscard]] inline auto as(this std::expected<QJsonValue, EvalError>& self) -> std::expected<T, EvalError>
{
    if (!self)
        return std::unexpected(self.error());
    auto base = json_query::detail::as_core<T>(*self);
    return base.transform_error(json_query::ErrorAdapter<EvalError>::from);
}

template <json_query::JsonTarget T>
[[nodiscard]] inline auto as(this const std::expected<QJsonValue, EvalError>& self) -> std::expected<T, EvalError>
{
    if (!self)
        return std::unexpected(self.error());
    auto base = json_query::detail::as_core<T>(*self);
    return base.transform_error(json_query::ErrorAdapter<EvalError>::from);
}

template <json_query::JsonTarget T>
[[nodiscard]] inline auto as(this std::expected<QJsonValue, EvalError>&& self) -> std::expected<T, EvalError>
{
    if (!self)
        return std::unexpected(std::move(self).error());
    auto base = json_query::detail::as_core<T>(*self);
    return base.transform_error(json_query::ErrorAdapter<EvalError>::from);
}

#else
// Fallback for compilers without explicit-this: ADL free function
template <json_query::JsonTarget T>
[[nodiscard]] inline auto as(const std::expected<QJsonValue, EvalError>& self) -> std::expected<T, EvalError>
{
    if (!self)
        return std::unexpected(self.error());
    auto base = json_query::detail::as_core<T>(*self);
    return base.transform_error(json_query::ErrorAdapter<EvalError>::from);
}
#endif

} // namespace json_query::json_pointer::detail
