// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

#include <expected>
#include <memory>

#include "JSONSchemaError.hpp"
#include "internal/SchemaNode.hpp"
#include "json-query/utils/JSONQueryError.hpp"

namespace json_query::json_schema
{

/**
 * @brief Compile a JSON Schema into an internal representation
 *
 * This function handles the schema compilation phase, converting a JSON Schema
 * document into an optimized internal representation for fast validation.
 *
 * @param schemaValue The JSON Schema (object or boolean)
 * @return Compiled schema or error
 */
[[nodiscard]] std::expected<std::shared_ptr<internal::CompiledSchema>, QueryError>
compileSchema(const QJsonValue& schemaValue);

} // namespace json_query::json_schema
