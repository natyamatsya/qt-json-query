// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-patch/JSONMergePatch.hpp"
#include "json-query/utils/detail/DocumentRoot.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_patch
{

// RFC 7386 §2 MergePatch(Target, Patch), verbatim. Recursion depth is bounded
// by the *patch* depth (caller-supplied; Qt's JSON parser enforces its own
// nesting limit for parsed documents).
QJsonValue merge_patch(const QJsonValue& target, const QJsonValue& patch) noexcept
{
    if (!patch.isObject())
        return patch.isUndefined() ? QJsonValue{} : patch; // wholesale replacement

    // Copy-init, not brace-init (ADR-001)
    const QJsonObject patchObj = patch.toObject();
    QJsonObject       result   = target.isObject() ? target.toObject() : QJsonObject{};

    for (auto it = patchObj.constBegin(); it != patchObj.constEnd(); ++it)
    {
        if (it.value().isNull())
            result.remove(it.key());
        else
            result.insert(it.key(), merge_patch(result.value(it.key()), it.value()));
    }
    return QJsonValue{result};
}

QJsonDocument merge_patch(const QJsonDocument& target, const QJsonDocument& patch) noexcept
{
    const auto result{merge_patch(utils::detail::unwrapRoot(target), utils::detail::unwrapRoot(patch))};
    QJsonDocument out;
    // A document-rooted patch is an object, array, or null, so the result is
    // always representable; the guard is belt-and-suspenders.
    if (!utils::detail::commitRoot(out, result))
        return QJsonDocument{};
    return out;
}

} // namespace json_query::json_patch
