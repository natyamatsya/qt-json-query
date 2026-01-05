// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonValue>

#include <vector>
#include <cstddef>
#include <expected>

#include "JSONSchemaError.hpp"
#include "json-query/json-pointer/JSONPointer.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

namespace json_query::json_schema
{

/**
 * @brief Represents a single validation error with location information
 *
 * Provides JSON Pointer locations for both the failing instance data and
 * the schema location that caused the failure. The instanceLocation can
 * be used to navigate directly to the failing value.
 */
struct ValidationError
{
    QString   instanceLocation; // JSON Pointer to failing data (e.g., "/address/zip")
    QString   schemaLocation;   // JSON Pointer within schema (e.g., "#/properties/address")
    QString   message;          // Human-readable description
    EvalError code;             // Machine-readable error code

    // For nested errors (e.g., from allOf/anyOf)
    std::vector<ValidationError> nested{};

    ValidationError() = default;

    ValidationError(QString instLoc, QString schemaLoc, QString msg, EvalError c)
        : instanceLocation(std::move(instLoc)), schemaLocation(std::move(schemaLoc)), message(std::move(msg)), code(c)
    {
    }

    /**
     * @brief Get a JSONPointer for navigating to the error location in the instance
     *
     * @return JSONPointer if instanceLocation is valid, nullopt for root ("")
     *
     * @example
     * @code
     * if (auto ptr = error.instancePointer()) {
     *     auto failingValue = ptr->evaluate(instance);
     * }
     * @endcode
     */
    [[nodiscard]] std::optional<json_pointer::JSONPointer> instancePointer() const
    {
        if (instanceLocation.isEmpty())
            return std::nullopt; // Root location

        auto result{json_pointer::JSONPointer::create(instanceLocation)};
        if (result)
            return *result;
        return std::nullopt;
    }

    /**
     * @brief Navigate directly to the failing value in the instance
     *
     * @param instance The JSON instance that was validated
     * @return The failing value, or nullopt if navigation fails
     */
    [[nodiscard]] std::optional<QJsonValue> navigateTo(const QJsonValue& instance) const
    {
        if (instanceLocation.isEmpty())
            return instance; // Root location

        auto ptr{instancePointer()};
        if (!ptr)
            return std::nullopt;

        auto result{ptr->evaluate(instance)};
        if (result)
            return *result;
        return std::nullopt;
    }

    /**
     * @brief Convert this error to a JSON object (for machine-readable output)
     */
    [[nodiscard]] QJsonObject toJson() const
    {
        using json_query::literals::operator""_qt_s;

        QJsonObject obj{};
        obj[u"instanceLocation"_qt_s] = instanceLocation;
        obj[u"schemaLocation"_qt_s]   = schemaLocation;
        obj[u"message"_qt_s]          = message;
        obj[u"code"_qt_s]             = static_cast<int>(code);

        if (!nested.empty())
        {
            QJsonArray nestedArr{};
            for (const auto& err : nested)
                nestedArr.append(err.toJson());
            obj[u"nested"_qt_s] = nestedArr;
        }

        return obj;
    }
};

/**
 * @brief Result of validating a JSON instance against a schema
 *
 * Contains success/failure status and detailed error information.
 * Follows the pattern established by std::expected but provides
 * richer error details for validation failures.
 */
class ValidationResult
{
  public:
    ValidationResult() = default;

    /**
     * @brief Check if validation succeeded
     */
    [[nodiscard]] bool isValid() const noexcept { return m_errors.empty(); }

    /**
     * @brief Boolean conversion for use in conditionals
     */
    [[nodiscard]] explicit operator bool() const noexcept { return isValid(); }

    /**
     * @brief Get the number of validation errors
     */
    [[nodiscard]] std::size_t errorCount() const noexcept { return m_errors.size(); }

    /**
     * @brief Get all validation errors
     */
    [[nodiscard]] const std::vector<ValidationError>& errors() const noexcept { return m_errors; }

    /**
     * @brief Get the first error message (empty if valid)
     */
    [[nodiscard]] QString firstError() const
    {
        if (m_errors.empty())
            return {};
        return m_errors.front().message;
    }

    /**
     * @brief Convert to machine-readable JSON output format
     *
     * Follows JSON Schema output format specification
     */
    [[nodiscard]] QJsonObject toJson() const
    {
        using json_query::literals::operator""_qt_s;

        QJsonObject result{};
        result[u"valid"_qt_s] = isValid();

        if (!isValid())
        {
            QJsonArray errorsArr{};
            for (const auto& err : m_errors)
                errorsArr.append(err.toJson());
            result[u"errors"_qt_s] = errorsArr;
        }

        return result;
    }

    /**
     * @brief Convert to human-readable string
     */
    [[nodiscard]] QString toString() const
    {
        using json_query::literals::operator""_qt_s;

        if (isValid())
            return u"Valid"_qt_s;

        QString result = QString(u"Invalid: %1 error(s)\n").arg(m_errors.size());
        for (const auto& err : m_errors)
            result += QString(u"  - %1 at %2\n").arg(err.message, err.instanceLocation);
        return result;
    }

    /**
     * @brief Add an error to the result
     */
    void addError(ValidationError error) { m_errors.push_back(std::move(error)); }

    /**
     * @brief Add an error with individual components
     */
    void addError(QString instanceLocation, QString schemaLocation, QString message, EvalError code)
    {
        m_errors.emplace_back(std::move(instanceLocation), std::move(schemaLocation), std::move(message), code);
    }

    /**
     * @brief Merge errors from another result
     */
    void merge(const ValidationResult& other)
    {
        m_errors.insert(m_errors.end(), other.m_errors.begin(), other.m_errors.end());
    }

    /**
     * @brief Clear all errors
     */
    void clear() { m_errors.clear(); }

  private:
    std::vector<ValidationError> m_errors{};
};

} // namespace json_query::json_schema
