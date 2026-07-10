// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include <QJsonDocument>
#include <QJsonValue>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_patch
{

/**
 * @brief Apply an RFC 7386 JSON Merge Patch to @p target.
 *
 * Merge patches are total: every input combination has a defined result, so
 * there is no error path. Patch object members merge recursively into the
 * target; an explicit null removes the member; any non-object patch value
 * (including arrays) replaces the target wholesale (RFC 7386 §2).
 *
 * @code
 * // {"title":"Hello","author":{"name":"A"}} + {"author":{"name":null},"tags":["x"]}
 * //   -> {"title":"Hello","author":{},"tags":["x"]}
 * const auto result{merge_patch(target, patch)};
 * @endcode
 *
 * @param target The document to patch (any JSON value).
 * @param patch  The merge patch (any JSON value).
 * @return The patched value.
 */
[[nodiscard]] QJsonValue merge_patch(const QJsonValue& target, const QJsonValue& patch) noexcept;

/**
 * @brief Document convenience overload: applies the patch document's root to
 *        the target document's root.
 *
 * Since a patch parsed from a JSON document is always an object or array, the
 * result is representable as a QJsonDocument for every input (a non-object
 * patch replaces the target wholesale).
 */
[[nodiscard]] QJsonDocument merge_patch(const QJsonDocument& target, const QJsonDocument& patch) noexcept;

} // namespace json_query::json_patch
