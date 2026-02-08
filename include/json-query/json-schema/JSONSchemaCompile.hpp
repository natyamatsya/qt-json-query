// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QJsonValue>

#include <expected>
#include <memory>

#include "JSONSchema.hpp"
#include "JSONSchemaError.hpp"
#include "internal/SchemaNode.hpp"
#include "json-query/utils/JSONError.hpp"

namespace json_query::json_schema
{

/**
 * @brief Compile a JSON Schema into an internal representation
 *
 * This function handles the schema compilation phase, converting a JSON Schema
 * document into an optimized internal representation for fast validation.
 *
 * @param schemaValue The JSON Schema (object or boolean)
 * @param fetcher Optional callback for resolving remote $ref URIs
 * @return Compiled schema or error
 */
[[nodiscard]] std::expected<std::shared_ptr<internal::CompiledSchema>, Error>
compileSchema(const QJsonValue& schemaValue, SchemaFetcher fetcher = {}, SchemaOptions options = {});

} // namespace json_query::json_schema
