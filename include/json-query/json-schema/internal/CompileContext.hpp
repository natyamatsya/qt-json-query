// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/utils/QtStringLiterals.hpp"

#include <QString>
#include <QStringView>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_schema::internal
{

/**
 * @brief Context for schema compilation
 *
 * Maintains state during recursive schema compilation.
 */
/// Callback type for resolving remote schema URIs (non-owning reference in context)
using SchemaFetcherFn = std::function<std::optional<QJsonValue>(const QString& uri)>;

struct CompileContext
{
    std::vector<SchemaNode>&                  nodes;
    std::unordered_map<QString, std::size_t>& anchors;
    std::unordered_map<QString, std::size_t>& dynamicAnchors;
    QString                                   basePath{};
    QString                                   baseUri{};

    /// Non-owning pointer to the schema fetcher (null if no remote resolution)
    const SchemaFetcherFn* fetcher{nullptr};

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

    /// Whether the validation vocabulary is active (default true for standard 2020-12).
    /// Set to false when the schema's $schema metaschema declares $vocabulary
    /// without https://json-schema.org/draft/2020-12/vocab/validation.
    bool validationVocabActive{true};

    /// Whether prefixItems is supported (2020-12+). Set to false for older drafts
    /// where prefixItems didn't exist and should be treated as an unknown keyword.
    bool prefixItemsSupported{true};

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
