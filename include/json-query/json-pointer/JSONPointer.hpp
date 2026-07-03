// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

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

/**
 * @brief Compiled RFC 6901 JSON Pointer.
 *
 * Create once via create(), then evaluate against any number of documents.
 *
 * Thread safety: a compiled JSONPointer is immutable; calling evaluate()
 * concurrently on the same instance from multiple threads is safe. create()
 * is also safe to call concurrently.
 */
class JSONPointer
{
  public:
    using ParseResult = std::expected<JSONPointer, json_query::Error>;
    using EvalResult  = std::expected<QJsonValue, json_query::Error>;

    /**
     * @brief Compile a JSON Pointer (e.g. "/foo/0/bar"; "" addresses the root).
     * @return The compiled pointer, or an Error with
     *         ErrorDomain::PointerParse on invalid RFC 6901 syntax.
     */
    static ParseResult create(QStringView pointer) noexcept;

    /**
     * @brief Resolve the pointer against a document.
     *
     * Unlike JSONPath, a JSON Pointer addresses exactly one location, so a
     * missing target IS an error: returns Error (ErrorDomain::PointerEval)
     * with code KeyNotFound / IndexOutOfRange / TypeMismatch* and the failing
     * token index in Error::detail. On success returns the referenced value
     * (which may be a JSON null).
     */
    [[nodiscard]] EvalResult evaluate(const QJsonDocument&) const;
    [[nodiscard]] EvalResult evaluate(const QJsonValue&) const;

    /// The pointer in its original RFC 6901 string form.
    [[nodiscard]] QString to_string() const;

  private:
    JSONPointer() = default; // internal default ctor for factory

    std::vector<Token> m_tokens;
};

} // namespace json_query::json_pointer
