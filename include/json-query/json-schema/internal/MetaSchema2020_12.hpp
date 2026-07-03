// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include <QJsonValue>
#include <QString>
#include <optional>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_schema::internal
{

/// Look up a built-in 2020-12 meta-schema by URI.
/// Returns the schema as a QJsonValue if found, std::nullopt otherwise.
[[nodiscard]] std::optional<QJsonValue> lookupBuiltinSchema(const QString& uri);

} // namespace json_query::json_schema::internal
