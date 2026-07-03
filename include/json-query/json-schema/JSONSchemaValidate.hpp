// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include "JSONSchemaResult.hpp"
#include "internal/SchemaNode.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_schema
{

/**
 * @brief Validate a JSON instance against a compiled schema
 *
 * @param schema The compiled schema
 * @param instance The JSON value to validate
 * @return ValidationResult with errors if any
 */
[[nodiscard]] ValidationResult validateInstance(const internal::CompiledSchema& schema, const QJsonValue& instance);

/**
 * @brief Quick validation check (stops at first error)
 *
 * @param schema The compiled schema
 * @param instance The JSON value to validate
 * @return true if valid, false otherwise
 */
[[nodiscard]] bool isInstanceValid(const internal::CompiledSchema& schema, const QJsonValue& instance);

} // namespace json_query::json_schema
