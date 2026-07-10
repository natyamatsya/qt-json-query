// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT
#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::utils::detail
{

// The single home for the QJsonDocument <-> root-QJsonValue conversion used
// by every module that offers QJsonDocument overloads. A QJsonDocument can
// only hold an object or array root (or be null); the scalar-root gap is the
// reason json_pointer::EvalError::DocumentRootNotContainer exists.

/// The document's root as a QJsonValue (JSON null for a null document).
[[nodiscard]] inline QJsonValue unwrapRoot(const QJsonDocument& doc) noexcept
{
    if (doc.isObject())
        return QJsonValue{doc.object()};
    if (doc.isArray())
        return QJsonValue{doc.array()};
    return QJsonValue{};
}

/// Write @p root back into @p doc. Returns false — leaving @p doc untouched —
/// when @p root is a scalar, which QJsonDocument cannot represent (callers
/// map this to DocumentRootNotContainer). A null root yields a null document.
[[nodiscard]] inline bool commitRoot(QJsonDocument& doc, const QJsonValue& root) noexcept
{
    if (root.isObject())
        doc.setObject(root.toObject());
    else if (root.isArray())
        doc.setArray(root.toArray());
    else if (root.isNull())
        doc = QJsonDocument{};
    else
        return false;
    return true;
}

} // namespace json_query::utils::detail
