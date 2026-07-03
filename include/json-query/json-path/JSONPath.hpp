// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

// ────────────────────────────── Qt
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonArray>
#include <QString>
#include <QStringView>

// ────────────────────────────── STL / C++23
#include <expected>
#include <memory>

// ────────────────────────────── Project
#include "json-query/utils/JSONError.hpp"

// ======================================================================
//  JSONPath
// ======================================================================
#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path
{

namespace detail
{
struct JSONPathImpl; // compiled state (tokens, filters) — internal
}

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
     *         invalid RFC 9535 syntax.
     *
     * Exception policy: the library never throws — all recoverable errors are
     * reported via std::expected. noexcept is therefore a statement that
     * memory exhaustion (the only conceivable throw, from the allocator) is
     * treated as fatal (std::terminate), matching Qt's own behavior.
     */
    static ParseResult create(QStringView path) noexcept;

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
    /// The path in its original RFC 9535 string form.
    [[nodiscard]] QString to_string() const;

    //  Moves are cheap; copies clone the compiled state (defined in .cpp,
    //  where the pimpl is a complete type)
    JSONPath(JSONPath&&) noexcept;
    JSONPath(const JSONPath&);
    JSONPath& operator=(JSONPath&&) noexcept;
    JSONPath& operator=(const JSONPath&);
    ~JSONPath();

  private:
    // Private pimpl ctor — used only by the factory
    explicit JSONPath(std::unique_ptr<const detail::JSONPathImpl> impl) noexcept;

    std::unique_ptr<const detail::JSONPathImpl> m_impl;
}; // end class JSONPath

} // namespace json_query::json_path
