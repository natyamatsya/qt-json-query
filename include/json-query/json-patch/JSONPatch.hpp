// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <expected>
#include <vector>

#include "json-query/json-patch/JSONPatchParsing.hpp"
#include "json-query/utils/JSONError.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_patch
{

/**
 * @brief Compiled RFC 6902 JSON Patch.
 *
 * Create once via create() (which validates the whole patch document and
 * compiles every pointer), then apply against any number of documents.
 *
 * Thread safety: a compiled JSONPatch is immutable; calling apply()
 * concurrently on the same instance from multiple threads is safe. create()
 * is also safe to call concurrently.
 */
class JSONPatch
{
  public:
    using ParseResult    = std::expected<JSONPatch, json_query::Error>;
    using ApplyResult    = std::expected<QJsonDocument, json_query::Error>;
    using ApplyValueResult = std::expected<QJsonValue, json_query::Error>;

    /**
     * @brief Compile a JSON Patch from its array form (RFC 6902 §3).
     *
     * Validation is eager: unknown operation names, missing required members
     * ("op", "path", "value", "from") and invalid pointers are all rejected
     * here, so apply() only ever fails on document-dependent conditions.
     *
     * @return The compiled patch, or an Error (ErrorDomain::PatchParse) with
     *         the index of the offending operation in Error::detail.
     *
     * Exception policy: the library never throws — all recoverable errors are
     * reported via std::expected. noexcept is therefore a statement that
     * memory exhaustion (the only conceivable throw, from the allocator) is
     * treated as fatal (std::terminate), matching Qt's own behavior.
     */
    static ParseResult create(const QJsonArray& patch) noexcept;
    static ParseResult create(const QJsonDocument& patch) noexcept;

    /**
     * @brief Apply the patch to a document, returning the patched document.
     *
     * Application is atomic (RFC 6902 §5): operations are applied in order
     * against a working copy, and the result is returned only if every
     * operation succeeded — on error the input is never partially patched.
     *
     * Errors: patch-specific failures (test mismatch, move into own
     * descendant) use ErrorDomain::PatchEval; failures of the underlying
     * pointer operations keep their PointerEval domain and code. In both
     * cases Error::detail holds the index of the failing operation (NOT a
     * pointer token index).
     *
     * @code
     * auto patch{JSONPatch::create(patchArray)};
     * if (auto result{patch->apply(doc)})
     *     doc = *result;
     * else
     *     qWarning() << result.error().formatted_message();
     * @endcode
     */
    [[nodiscard]] ApplyResult apply(const QJsonDocument& document) const noexcept;
    [[nodiscard]] ApplyValueResult apply(const QJsonValue& root) const noexcept;

    /**
     * @brief Convenience wrapper: apply and, on success, assign the result
     *        back into @p document. On error @p document is untouched.
     *
     * The typed-root overloads (QJsonObject& / QJsonArray&) additionally
     * fail with RootTypeMismatch when the patched result is not the fixed
     * container kind (e.g. an add with path "" replacing the root with an
     * array while patching a QJsonObject).
     */
    [[nodiscard]] std::expected<void, json_query::Error> applyInPlace(QJsonDocument& document) const noexcept;
    [[nodiscard]] std::expected<void, json_query::Error> applyInPlace(QJsonObject& root) const noexcept;
    [[nodiscard]] std::expected<void, json_query::Error> applyInPlace(QJsonArray& root) const noexcept;

    /// Number of operations in the compiled patch.
    [[nodiscard]] qsizetype size() const noexcept { return static_cast<qsizetype>(m_ops.size()); }
    /// True if the patch contains no operations (applies as a no-op).
    [[nodiscard]] bool isEmpty() const noexcept { return m_ops.empty(); }

  private:
    // No pimpl here on purpose: Op holds only first-party types (JSONPointer,
    // QJsonValue), so nothing third-party leaks into the public surface and
    // create() stays free of the extra pimpl allocation — the same reasoning
    // as JSONPointer's inline token vector.
    JSONPatch() = default; // internal default ctor for factory

    std::vector<detail::Op> m_ops;
};

} // namespace json_query::json_patch
