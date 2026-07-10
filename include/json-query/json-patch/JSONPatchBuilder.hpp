// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once
#include <QJsonArray>
#include <QJsonValue>
#include <QStringView>

#include "json-query/json-patch/JSONPatch.hpp"
#include "json-query/json-pointer/JSONPointer.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_patch
{

/**
 * @brief Fluent construction of RFC 6902 patches — batching without
 *        hand-assembling the QJson operations array.
 *
 * The builder assembles the standard wire format (toJson()) and build()
 * compiles it through JSONPatch::create(), so a built patch is validated by
 * exactly the same rules as a parsed one and errors carry the offending
 * operation index in Error::detail. Overloads taking a compiled JSONPointer
 * are correct by construction (the pointer re-encodes canonically); the
 * QStringView overloads accept raw pointer strings, validated at build().
 *
 * @code
 * auto patch{JSONPatchBuilder{}
 *                .test(u"/version", 3)
 *                .replace(u"/name", "new")
 *                .add(mappings / u"-", mapping) // composed pointer
 *                .build()};
 * if (patch)
 *     patch->applyInPlace(doc);
 * @endcode
 */
class JSONPatchBuilder
{
  public:
    JSONPatchBuilder& add(const json_pointer::JSONPointer& path, const QJsonValue& value);
    JSONPatchBuilder& add(QStringView path, const QJsonValue& value);

    JSONPatchBuilder& replace(const json_pointer::JSONPointer& path, const QJsonValue& value);
    JSONPatchBuilder& replace(QStringView path, const QJsonValue& value);

    JSONPatchBuilder& remove(const json_pointer::JSONPointer& path);
    JSONPatchBuilder& remove(QStringView path);

    JSONPatchBuilder& move(const json_pointer::JSONPointer& from, const json_pointer::JSONPointer& path);
    JSONPatchBuilder& move(QStringView from, QStringView path);

    JSONPatchBuilder& copy(const json_pointer::JSONPointer& from, const json_pointer::JSONPointer& path);
    JSONPatchBuilder& copy(QStringView from, QStringView path);

    JSONPatchBuilder& test(const json_pointer::JSONPointer& path, const QJsonValue& value);
    JSONPatchBuilder& test(QStringView path, const QJsonValue& value);

    /// Compile the assembled operations (JSONPatch::create rules and errors).
    [[nodiscard]] JSONPatch::ParseResult build() const noexcept;

    /// The assembled operations in RFC 6902 wire format.
    [[nodiscard]] const QJsonArray& toJson() const noexcept { return m_ops; }

    /// Number of operations assembled so far.
    [[nodiscard]] qsizetype size() const noexcept { return m_ops.size(); }

  private:
    QJsonArray m_ops;
};

} // namespace json_query::json_patch
