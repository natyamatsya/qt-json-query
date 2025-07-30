// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once
#include <QJsonValue>
#include <optional>
#include <expected>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "json-query/json-pointer/JSONPointerParsing.hpp"
#include "json-query/json-pointer/JSONPointerEvaluation.hpp"

#include <vector> // added include for std::vector

#include "json-query/utils/JSONQueryError.hpp"

namespace json_query::json_pointer
{

class JSONPointer
{
  public:
    // Factory function mirroring JSONPath
    using ParseResult = std::expected<JSONPointer, json_query::QueryError>;
    using EvalResult  = std::expected<QJsonValue, json_query::QueryError>;

    // Note: Internal error types are not exposed in the public API
    // All error handling should use the QueryError type

    // Factory function mirroring JSONPath
    static ParseResult create(QStringView pointer);

    // Detailed-error variants
    [[nodiscard]] EvalResult evaluate(const QJsonDocument&) const;
    [[nodiscard]] EvalResult evaluate(const QJsonValue&) const;

    [[nodiscard]] QString to_string() const;

  private:
    JSONPointer() = default; // internal default ctor for factory

    std::vector<Token> m_tokens;
};

} // namespace json_query::json_pointer
