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
 * @brief Options for JSONPointer::set().
 */
struct WriteOptions
{
    /// Create missing intermediate containers along the pointer ("mkdir -p").
    /// The created container's type is chosen by the token that will be
    /// applied inside it: array for index 0 or the "-" append designator,
    /// object otherwise. A JSON null in the path counts as "nothing here yet"
    /// and is replaced by a created container; any other existing value is
    /// never overwritten (the write fails with a TypeMismatch error instead).
    bool createIntermediates{false};
};

/**
 * @brief Compiled RFC 6901 JSON Pointer.
 *
 * Create once via create(), then evaluate against any number of documents.
 *
 * Thread safety: a compiled JSONPointer is immutable; calling evaluate()
 * concurrently on the same instance from multiple threads is safe (the write
 * methods mutate only the passed document, never the pointer). create() is
 * also safe to call concurrently.
 */
class JSONPointer
{
  public:
    using ParseResult  = std::expected<JSONPointer, json_query::Error>;
    using EvalResult   = std::expected<QJsonValue, json_query::Error>;
    using WriteResult  = std::expected<void, json_query::Error>;
    using RemoveResult = std::expected<QJsonValue, json_query::Error>;

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

    /**
     * @brief Insert @p value at the pointer target (RFC 6902 "add" semantics).
     *
     * The parent of the target must exist. On an object the member is
     * inserted or replaced; on an array the index must be in [0, size]
     * (index == size and the "-" token append). The empty pointer ""
     * replaces the whole document.
     *
     * All write methods provide the strong guarantee: on error the document
     * is left untouched. An Undefined @p value is stored as JSON null.
     *
     * @return Nothing on success, or an Error (ErrorDomain::PointerEval) with
     *         the failing token index in Error::detail.
     *
     * @code
     * auto ptr{JSONPointer::create("/books/-")};
     * if (auto r{ptr->add(doc, newBook)}; !r)
     *     qWarning() << r.error().formatted_message();
     * @endcode
     */
    [[nodiscard]] WriteResult add(QJsonDocument& doc, const QJsonValue& value) const noexcept;
    [[nodiscard]] WriteResult add(QJsonValue& root, const QJsonValue& value) const noexcept;

    /**
     * @brief Replace the existing value at the pointer target (RFC 6902
     *        "replace" semantics).
     *
     * Unlike add(), the target itself must already exist (missing object
     * member: KeyNotFound; array index or "-": IndexOutOfRange). The empty
     * pointer "" replaces the whole document. Strong guarantee: on error the
     * document is left untouched.
     */
    [[nodiscard]] WriteResult replace(QJsonDocument& doc, const QJsonValue& value) const noexcept;
    [[nodiscard]] WriteResult replace(QJsonValue& root, const QJsonValue& value) const noexcept;

    /**
     * @brief Remove the value at the pointer target (RFC 6902 "remove"
     *        semantics).
     *
     * The target must exist; removing from an array shifts later elements
     * left. Removing the root ("") is an error (CannotRemoveRoot). Strong
     * guarantee: on error the document is left untouched.
     *
     * @return The removed value on success (handy as a "take" operation).
     */
    [[nodiscard]] RemoveResult remove(QJsonDocument& doc) const noexcept;
    [[nodiscard]] RemoveResult remove(QJsonValue& root) const noexcept;

    /**
     * @brief Convenience upsert: add(), optionally creating missing
     *        intermediate containers (see WriteOptions::createIntermediates).
     *
     * With default options this is exactly add(). Strong guarantee: on error
     * the document is left untouched.
     *
     * @code
     * // Settings-backend style write into a possibly-empty document:
     * auto ptr{JSONPointer::create("/ui/theme/accent")};
     * auto r{ptr->set(doc, "teal", {.createIntermediates = true})};
     * @endcode
     */
    [[nodiscard]] WriteResult set(QJsonDocument& doc, const QJsonValue& value, WriteOptions options = {}) const noexcept;
    [[nodiscard]] WriteResult set(QJsonValue& root, const QJsonValue& value, WriteOptions options = {}) const noexcept;

  private:
    // No pimpl here on purpose: Token is first-party (no third-party headers
    // leak into the public surface), and an inline member keeps create()
    // free of the extra pimpl allocation. JSONPath, whose compile machinery
    // would drag CTRE into consumer TUs, is pimpl'd instead.
    JSONPointer() = default; // internal default ctor for factory

    std::vector<Token> m_tokens;
};

} // namespace json_query::json_pointer
