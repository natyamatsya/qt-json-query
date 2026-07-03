// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once
#include <QJsonDocument>
#include <QJsonValue>
#include <QString>
#include <QStringView>

#include <expected>
#include <vector>

#include "json-query/json-pointer/JSONPointerParsing.hpp"
#include "json-query/utils/JSONError.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_pointer
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
     *
     * Exception policy: the library never throws — all recoverable errors are
     * reported via std::expected. noexcept is therefore a statement that
     * memory exhaustion (the only conceivable throw, from the allocator) is
     * treated as fatal (std::terminate), matching Qt's own behavior.
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
    // No pimpl here on purpose: Token is first-party (no third-party headers
    // leak into the public surface), and an inline member keeps create()
    // free of the extra pimpl allocation. JSONPath, whose compile machinery
    // would drag CTRE into consumer TUs, is pimpl'd instead.
    JSONPointer() = default; // internal default ctor for factory

    std::vector<Token> m_tokens;
};

} // namespace json_query::json_pointer
