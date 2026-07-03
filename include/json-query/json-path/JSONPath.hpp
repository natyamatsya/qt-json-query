// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

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
#include "json-query/json-path/JSONPathOption.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include "json-query/utils/JSONError.hpp"
#include "json-query/json-pointer/JSONPointer.hpp"

// ======================================================================
//  JSONPath
// ======================================================================
namespace json_query::json_path
{

/**
 * @brief Compiled RFC 9535 JSONPath query.
 *
 * Create once via create(), then evaluate against any number of documents.
 *
 * Thread safety: a compiled JSONPath is immutable; calling evaluate() /
 * evaluateSingle() concurrently on the same instance from multiple threads
 * is safe. create() is also safe to call concurrently.
 */
class JSONPath
{
  public:
    // -----------------------------------------------------------------
    //  Factory (replaces throwing constructor)
    // -----------------------------------------------------------------
    using ParseResult = std::expected<JSONPath, json_query::Error>;

    /**
     * @brief Compile a JSONPath expression (e.g. "$.store.book[?(@.price < 10)]").
     * @return The compiled path, or an Error with ErrorDomain::PathParse on
     *         invalid RFC 9535 syntax. Never throws.
     */
    static ParseResult create(QStringView path);

    // -----------------------------------------------------------------
    //  Evaluation API with error reporting (std::expected)
    // -----------------------------------------------------------------
    using EvalResult      = std::expected<QJsonValue, json_query::Error>;
    using EvalArrayResult = std::expected<QJsonArray, json_query::Error>;

    /**
     * @brief Evaluate and return the full nodelist (preferred API).
     *
     * Returns every matched node as an element of the result array, in
     * document order. Zero matches yield an empty array — including for
     * definite paths whose target is absent (a missing node is an empty
     * nodelist, not an error). Duplicate values at distinct locations are
     * distinct nodes and all appear. An Error is returned only for
     * evaluation failures (ErrorDomain::PathEval), not for "not found".
     *
     * Unlike evaluateSingle(), the result is never ambiguous: a single
     * matched array value arrives as the array's sole element.
     */
    [[nodiscard]] EvalArrayResult evaluate(const QJsonDocument& doc) const;
    [[nodiscard]] EvalArrayResult evaluate(const QJsonValue& value) const;

    /**
     * @brief Evaluate and return a "squashed" nodelist as one QJsonValue.
     *
     * - Definite path (only key/index selectors) whose target exists:
     *   returns the matched value directly.
     * - Definite path whose target is absent: returns a QJsonValue wrapping
     *   an empty QJsonArray (the empty nodelist) — NOT an error.
     * - Non-definite path (contains a wildcard, slice, filter, recursive
     *   descent, or multi-key union): returns a QJsonValue wrapping the
     *   QJsonArray of all matches, even when there is exactly one.
     *
     * @warning The squash is inherently ambiguous: a definite path matching
     * a value that IS an array (possibly empty) cannot be distinguished
     * from the nodelist encodings above. Use evaluate() when the matched
     * values may themselves be arrays.
     */
    [[nodiscard]] EvalResult evaluateSingle(const QJsonDocument& doc) const;
    [[nodiscard]] EvalResult evaluateSingle(const QJsonValue& value) const;

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
             std::vector<json_query::json_path::Token> tokens,
             bool                                      definite) noexcept
        : m_func(func), m_originalPath(std::move(original)), m_tokens(std::move(tokens)), m_definite(definite)
    {
    }

    // -----------------------------------------------------------------
    //  Data members
    // -----------------------------------------------------------------
    json_query::json_path::FunctionType       m_func{json_query::json_path::FunctionType::None};
    QString                                   m_originalPath;
    std::vector<json_query::json_path::Token> m_tokens;
    bool                                      m_definite{false};
}; // end class JSONPath

} // namespace json_query::json_path
