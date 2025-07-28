// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringView>
#include <type_traits>

#include "../json-path/JSONPathEvalError.hpp"
#include "../json-pointer/JSONPointerEvaluation.hpp"

namespace json_query::utils
{

// Template helper for static_assert
template <class>
constexpr bool always_false = false;

// Forward declarations
namespace json_path
{
class JSONPath;
}
namespace json_pointer
{
class JSONPointer;
}

// Core implementation details
namespace detail
{
}

/**
 * @brief Safely converts a QJsonValue to the specified type
 * @tparam T The target type (QJsonArray, QJsonObject, QString, etc.)
 * @param value The QJsonValue to convert
 * @return std::expected<T, ErrorType> containing the converted value or an error
 *
 * This function works with both JSONPath and JSONPointer error types.
 *
 * Example usage:
 * @code
 * // With JSONPath errors
 * auto result1 = as<QJsonArray, json_query::json_path::EvalError>(someValue);
 *
 * // With JSONPointer errors
 * auto result2 = as<QJsonObject, json_query::json_pointer::detail::EvalError>(someValue);
 * @endcode
 */
template <typename T, typename ErrorType = json_query::json_path::EvalError>
std::expected<T, ErrorType> as(const QJsonValue& value)
{
    // First, convert using the default JSONPath error type
    auto result = as<T, json_query::json_path::EvalError>(value);

    if constexpr (std::is_same_v<ErrorType, json_query::json_path::EvalError>)
        return result;
    else if constexpr (std::is_same_v<ErrorType, json_query::json_pointer::detail::EvalError>)
        return result.transform_error(detail::to_pointer_error);
    else
        static_assert(always_false<ErrorType>, "Unsupported error type for as<T, ErrorType>");
}

// JSONPath-specific utilities
namespace json_path
{

/**
 * @brief Safely converts a QJsonValue to the specified type using JSONPath error handling
 * @tparam T The target type (QJsonArray, QJsonObject, QString, etc.)
 * @param value The QJsonValue to convert
 * @return std::expected<T, EvalError> containing the converted value or an error
 */
template <typename T>
std::expected<T, EvalError> as(const QJsonValue& value)
{
    return detail::as_impl<T, EvalError>(value);
}

/**
 * @brief Safely converts an EvalResult to the specified type
 * @tparam T The target type (QJsonArray, QJsonObject, QString, etc.)
 * @param result The EvalResult to convert
 * @return std::expected<T, EvalError> containing the converted value or an error
 *
 * This overload works directly with EvalResult for more ergonomic chaining.
 *
 * Example usage:
 * @code
 * auto result = jsonPath.evaluate(doc)
 *     .and_then(json_query::utils::json_path::as<QJsonArray>);
 * @endcode
 */
template <typename T>
std::expected<T, EvalError> as(const std::expected<QJsonValue, EvalError>& result)
{
    if (!result)
        return std::unexpected(result.error());
    return as<T>(*result);
}

} // namespace json_path

// JSONPointer-specific utilities
namespace json_pointer
{

/**
 * @brief Safely converts a QJsonValue to the specified type using JSONPointer error handling
 * @tparam T The target type (QJsonArray, QJsonObject, QString, etc.)
 * @param value The QJsonValue to convert
 * @return std::expected<T, detail::EvalError> containing the converted value or an error
 */
template <typename T>
std::expected<T, detail::EvalError> as(const QJsonValue& value)
{
    return detail::as_impl<T, detail::EvalError>(value);
}

/**
 * @brief Safely converts an EvalResult to the specified type
 * @tparam T The target type (QJsonArray, QJsonObject, QString, etc.)
 * @param result The EvalResult to convert
 * @return std::expected<T, detail::EvalError> containing the converted value or an error
 *
 * This overload works directly with EvalResult for more ergonomic chaining.
 *
 * Example usage:
 * @code
 * auto result = jsonPointer.evaluate(doc)
 *     .and_then(json_query::utils::json_pointer::as<QJsonObject>);
 * @endcode
 */
template <typename T>
std::expected<T, detail::EvalError> as(const std::expected<QJsonValue, detail::EvalError>& result)
{
    if (!result)
        return std::unexpected(result.error());
    return as<T>(*result);
}

} // namespace json_pointer

// Primary template for JSONPath EvalError
template <typename T>
template <typename T, typename ErrorType>
std::expected<T, ErrorType> as_impl(const QJsonValue& value)
{
    if constexpr (std::is_same_v<T, QJsonArray>)
    {
        return value.isArray() ? std::expected<T, ErrorType>{value.toArray()}
                               : std::unexpected(ErrorType::TypeMismatchArray);
    }
    else if constexpr (std::is_same_v<T, QJsonObject>)
    {
        return value.isObject() ? std::expected<T, ErrorType>{value.toObject()}
                                : std::unexpected(ErrorType::TypeMismatchObject);
    }
    else if constexpr (std::is_same_v<T, QString>)
    {
        return value.isString() ? std::expected<T, ErrorType>{value.toString()}
                                : std::unexpected(ErrorType::TypeMismatchObject);
    }
    else if constexpr (std::is_same_v<T, QStringView>)
    {
        return value.isString() ? std::expected<T, ErrorType>{QStringView{value.toString()}}
                                : std::unexpected(ErrorType::TypeMismatchObject);
    }
    else if constexpr (std::is_same_v<T, double>)
    {
        return value.isDouble() ? std::expected<T, ErrorType>{value.toDouble()}
                                : std::unexpected(ErrorType::TypeMismatchArray);
    }
    else if constexpr (std::is_same_v<T, int>)
    {
        return value.isDouble() ? std::expected<T, ErrorType>{static_cast<int>(value.toDouble())}
                                : std::unexpected(ErrorType::TypeMismatchArray);
    }
    else if constexpr (std::is_same_v<T, bool>)
    {
        return value.isBool() ? std::expected<T, ErrorType>{value.toBool()}
                              : std::unexpected(ErrorType::TypeMismatchObject);
    }
    else
    {
        static_assert(always_false<T>, "Unsupported type for as<T>");
    }
}

/**
 * @brief Converts an EvalError to a human-readable string
 * @tparam E The error enum type (JSONPath or JSONPointer EvalError)
 * @param error The error to convert
 * @return QString containing the error message
 *
 * @note This function uses the built-in to_string functions from the respective
 *       error enums to ensure consistent error message formatting.
 */
template <typename E>
[[nodiscard]] inline QString errorMessage(E error) noexcept
{
    if constexpr (std::is_same_v<E, json_query::json_path::EvalError>)
        return QString::fromUtf8(json_query::json_path::to_string(error).data());
    else if constexpr (std::is_same_v<E, json_query::json_pointer::detail::EvalError>)
        return QString::fromUtf8(json_query::json_pointer::detail::to_string(error).data());
    else
        static_assert(always_false<E>, "Unsupported error type for errorMessage");
}

} // namespace json_query::utils
