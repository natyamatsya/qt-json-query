// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <expected>
#include <functional>
#include <memory>
#include <optional>

#include "JSONSchemaError.hpp"
#include "JSONSchemaResult.hpp"
#include "json-query/utils/JSONQueryError.hpp"
#include "internal/SchemaNode.hpp"

namespace json_query::json_schema
{

/**
 * @brief Compiled JSON Schema for validation
 *
 * JSONSchema represents a compiled JSON Schema that can be used to validate
 * JSON instances. The schema is compiled once and can be reused for multiple
 * validations, making it efficient for repeated use.
 *
 * The class follows the same factory pattern as JSONPath and JSONPointer,
 * using std::expected for error handling.
 *
 * Thread-safety: After creation, JSONSchema is immutable and can be safely
 * used from multiple threads concurrently.
 *
 * @example
 * @code
 * auto schemaResult = JSONSchema::create(schemaObject);
 * if (schemaResult) {
 *     auto result = schemaResult->validate(instanceValue);
 *     if (result) {
 *         qDebug() << "Valid!";
 *     } else {
 *         for (const auto& err : result.errors()) {
 *             qDebug() << err.message;
 *         }
 *     }
 * }
 * @endcode
 */
/**
 * @brief Callback for resolving remote schema references
 *
 * When a JSON Schema contains a `$ref` pointing to an external URI,
 * the compiler calls this function to fetch the remote schema document.
 * Return std::nullopt if the URI cannot be resolved.
 *
 * @code
 * auto fetcher = [](const QString& uri) -> std::optional<QJsonValue> {
 *     auto response = httpGet(uri);
 *     if (!response)
 *         return std::nullopt;
 *     return QJsonDocument::fromJson(response->body).object();
 * };
 * auto schema = JSONSchema::create(schemaJson, std::move(fetcher));
 * @endcode
 */
using SchemaFetcher = std::function<std::optional<QJsonValue>(const QString& uri)>;

class JSONSchema
{
  public:
    // Type aliases matching JSONPath/JSONPointer patterns
    using ParseResult = std::expected<JSONSchema, json_query::QueryError>;

    /**
     * @brief Create a compiled schema from a JSON object
     *
     * @param schemaObject The JSON Schema as a QJsonObject
     * @return ParseResult containing the compiled schema or an error
     */
    static ParseResult create(const QJsonObject& schemaObject);
    static ParseResult create(const QJsonObject& schemaObject, SchemaFetcher fetcher);

    /**
     * @brief Create a compiled schema from a JSON document
     *
     * @param schemaDoc The JSON Schema as a QJsonDocument
     * @return ParseResult containing the compiled schema or an error
     */
    static ParseResult create(const QJsonDocument& schemaDoc);
    static ParseResult create(const QJsonDocument& schemaDoc, SchemaFetcher fetcher);

    /**
     * @brief Create a compiled schema from a JSON value (object or boolean)
     *
     * JSON Schema allows boolean schemas (true = accept all, false = reject all)
     *
     * @param schemaValue The JSON Schema as a QJsonValue
     * @return ParseResult containing the compiled schema or an error
     */
    static ParseResult create(const QJsonValue& schemaValue);
    static ParseResult create(const QJsonValue& schemaValue, SchemaFetcher fetcher);

    /**
     * @brief Validate a JSON value against this schema
     *
     * @param instance The JSON value to validate
     * @return ValidationResult containing success/failure and any errors
     */
    [[nodiscard]] ValidationResult validate(const QJsonValue& instance) const;

    /**
     * @brief Validate a JSON document against this schema
     *
     * @param doc The JSON document to validate
     * @return ValidationResult containing success/failure and any errors
     */
    [[nodiscard]] ValidationResult validate(const QJsonDocument& doc) const;

    /**
     * @brief Quick validation check (discards error details)
     *
     * More efficient when you only need to know if validation passed.
     *
     * @param instance The JSON value to validate
     * @return true if valid, false otherwise
     */
    [[nodiscard]] bool isValid(const QJsonValue& instance) const;

    /**
     * @brief Get the schema's $id if present
     */
    [[nodiscard]] QString schemaId() const;

    /**
     * @brief Get the schema's $schema dialect if present
     */
    [[nodiscard]] QString schemaVersion() const;

    /**
     * @brief Check if the schema was successfully compiled
     */
    [[nodiscard]] bool isCompiled() const noexcept { return m_compiled != nullptr; }

    // Move/copy (schema is immutable after creation, safe to share)
    JSONSchema(JSONSchema&&) noexcept            = default;
    JSONSchema(const JSONSchema&)                = default;
    JSONSchema& operator=(JSONSchema&&) noexcept = default;
    JSONSchema& operator=(const JSONSchema&)     = default;
    ~JSONSchema()                                = default;

  private:
    JSONSchema() = default;
    explicit JSONSchema(std::shared_ptr<const internal::CompiledSchema> compiled) : m_compiled(std::move(compiled)) {}

    std::shared_ptr<const internal::CompiledSchema> m_compiled;
};

} // namespace json_query::json_schema
