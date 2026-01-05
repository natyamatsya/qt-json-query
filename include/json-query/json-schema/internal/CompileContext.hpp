// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/SchemaNode.hpp"

#include <QString>
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
        if (baseUri.isEmpty())
            return anchorName;
        return baseUri + u"#"_qs + anchorName;
    }
};

} // namespace json_query::json_schema::internal
