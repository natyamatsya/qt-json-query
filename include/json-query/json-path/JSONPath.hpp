// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

// ────────────────────────────── Qt
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringView>
#include <QVector>
#include <vector>

// ────────────────────────────── STL / C++23
#include <expected>
#include <functional>
#include <optional>
#include <variant>
#include <cstdint>

// ────────────────────────────── Project
#include "JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPathExpected.hpp"
#include "json-query/json-path/JSONPathOption.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include "json-query/utils/JSONQueryError.hpp"
#include "json-query/json-pointer/JSONPointer.hpp"

using namespace Qt::StringLiterals;
// ======================================================================
//  JSONPath
// ======================================================================
namespace json_query::json_path
{

class JSONPath
{
  public:
    // -----------------------------------------------------------------
    //  Factory (replaces throwing constructor)
    // -----------------------------------------------------------------
    using Result = std::expected<JSONPath, json_query::QueryError>;

    static Result create(QStringView path);

    // -----------------------------------------------------------------
    //  Evaluation API with error reporting (std::expected)
    // -----------------------------------------------------------------
    using EvalResult      = std::expected<QJsonValue, json_query::QueryError>;
    using EvalArrayResult = std::expected<QJsonArray, json_query::QueryError>;

    [[nodiscard]] EvalResult evaluate(const QJsonDocument& doc) const;
    [[nodiscard]] EvalResult evaluate(const QJsonValue& value) const;

    [[nodiscard]] EvalArrayResult evaluateAll(const QJsonDocument& doc) const;
    [[nodiscard]] EvalArrayResult evaluateAll(const QJsonValue& value) const;

    // -----------------------------------------------------------------
    //  Other
    // -----------------------------------------------------------------
    [[nodiscard]] QString to_string() const { return m_originalPath; }

    //  Move / copy remain defaulted
    JSONPath(JSONPath&&) noexcept            = default;
    JSONPath(const JSONPath&)                = default;
    JSONPath& operator=(JSONPath&&) noexcept = default;
    JSONPath& operator=(const JSONPath&)     = default;

  private:
    // -----------------------------------------------------------------
    //  Private "data" ctor – used only by factory                     ★
    // -----------------------------------------------------------------
    JSONPath(json_query::json_path::FunctionType       func,
             QString                                   original,
             std::vector<json_query::json_path::Token> tokens) noexcept
        : m_func(func), m_originalPath(std::move(original)), m_tokens(std::move(tokens))
    {
    }

    // -----------------------------------------------------------------
    //  Data members
    // -----------------------------------------------------------------
    json_query::json_path::FunctionType       m_func{json_query::json_path::FunctionType::None};
    QString                                   m_originalPath;
    std::vector<json_query::json_path::Token> m_tokens;
}; // end class JSONPath

} // namespace json_query::json_path
