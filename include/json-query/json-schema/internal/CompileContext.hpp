// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

#include <QString>
#include <QStringView>
#include <unordered_map>
#include <vector>

namespace json_query::json_schema::internal
{

/**
 * @brief Context for schema compilation
 *
 * Maintains state during recursive schema compilation.
 */
struct CompileContext
{
    std::vector<SchemaNode>&                  nodes;
    std::unordered_map<QString, std::size_t>& anchors;
    std::unordered_map<QString, std::size_t>& dynamicAnchors;
    QString                                   basePath{};
    QString                                   baseUri{};

    /// Pending dynamic anchor registrations: {resourceUri, anchorName, nodeIndex}
    struct PendingDynAnchor
    {
        QString     resourceUri;
        QString     anchorName;
        std::size_t nodeIndex;
    };
    std::vector<PendingDynAnchor> pendingDynamicAnchors{};

    /// Resource JSON documents for sub-resource JSON Pointer resolution
    /// Maps $id → QJsonValue of the resource root
    std::unordered_map<QString, QJsonValue> resourceDocuments{};

    /**
     * @brief Add a node and return its index
     */
    [[nodiscard]] std::size_t addNode(SchemaNode node)
    {
        const auto index{nodes.size()};
        nodes.push_back(std::move(node));
        return index;
    }

    /**
     * @brief Get the full anchor key scoped to the current base URI
     */
    [[nodiscard]] QString scopedAnchorKey(const QString& anchorName) const
    {
        using json_query::literals::operator""_qt_s;

        if (baseUri.isEmpty())
            return anchorName;
        return baseUri + u"#"_qt_s + anchorName;
    }
};

} // namespace json_query::json_schema::internal
