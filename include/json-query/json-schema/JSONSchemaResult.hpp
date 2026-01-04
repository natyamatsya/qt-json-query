// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QString>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

#include <vector>
#include <cstddef>

#include "JSONSchemaError.hpp"

namespace json_query::json_schema
{

/**
 * @brief Represents a single validation error with location information
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
     * @brief Convert this error to a JSON object (for machine-readable output)
     */
    [[nodiscard]] QJsonObject toJson() const
    {
        QJsonObject obj{};
        obj[u"instanceLocation"_qs] = instanceLocation;
        obj[u"schemaLocation"_qs]   = schemaLocation;
        obj[u"message"_qs]          = message;
        obj[u"code"_qs]             = static_cast<int>(code);

        if (!nested.empty())
        {
            QJsonArray nestedArr{};
            for (const auto& err : nested)
                nestedArr.append(err.toJson());
            obj[u"nested"_qs] = nestedArr;
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
        QJsonObject result{};
        result[u"valid"_qs] = isValid();

        if (!isValid())
        {
            QJsonArray errorsArr{};
            for (const auto& err : m_errors)
                errorsArr.append(err.toJson());
            result[u"errors"_qs] = errorsArr;
        }

        return result;
    }

    /**
     * @brief Convert to human-readable string
     */
    [[nodiscard]] QString toString() const
    {
        if (isValid())
            return u"Valid"_qs;

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
