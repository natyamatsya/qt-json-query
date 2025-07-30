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

namespace json_query::json_pointer
{

class JSONPointer
{
  public:
    using ParseError = json_pointer::ParseError; // alias internal
    using EvalError  = json_pointer::EvalError;  // alias internal

    // Factory function mirroring JSONPath
    using ParseResult = std::expected<JSONPointer, ParseError>;
    using EvalResult  = std::expected<QJsonValue, EvalError>;

    // Factory function mirroring JSONPath
    static ParseResult create(QStringView pointer);

    // Detailed-error variants
    [[nodiscard]] EvalResult evaluate(const QJsonDocument&) const;
    [[nodiscard]] EvalResult evaluate(const QJsonValue&) const;

    [[nodiscard]] QString to_string() const;

  private:
    JSONPointer() = default; // internal default ctor for factory

    using Token = json_query::json_pointer::detail::Token;
    std::vector<Token> m_tokens; // QVector replaced with std::vector
};

} // namespace json_query::json_pointer
