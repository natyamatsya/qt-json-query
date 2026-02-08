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
#include "json-query/utils/JSONError.hpp"

#include <vector>

namespace json_query::json_pointer
{

class JSONPointer
{
  public:
    using ParseResult = std::expected<JSONPointer, json_query::Error>;
    using EvalResult  = std::expected<QJsonValue, json_query::Error>;

    static ParseResult create(QStringView pointer) noexcept;

    // Detailed-error variants
    [[nodiscard]] EvalResult evaluate(const QJsonDocument&) const;
    [[nodiscard]] EvalResult evaluate(const QJsonValue&) const;

    [[nodiscard]] QString to_string() const;

  private:
    JSONPointer() = default; // internal default ctor for factory

    std::vector<Token> m_tokens;
};

} // namespace json_query::json_pointer
