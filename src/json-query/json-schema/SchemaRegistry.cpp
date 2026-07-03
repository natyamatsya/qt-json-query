// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-schema/SchemaRegistry.hpp"

#include <QtCore/QJsonObject>

#include "json-query/utils/QtStringLiterals.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_schema
{

void SchemaRegistry::setFetcher(SchemaFetcher fetcher) { m_userFetcher = std::move(fetcher); }

JSONSchema::ParseResult SchemaRegistry::add(const QString& uri, const QJsonValue& schema, SchemaOptions options)
{
    // Cache the raw document for Tier 2
    m_documents[uri] = schema;

    auto compiled{JSONSchema::create(schema, makeCachingFetcher(), options)};
    if (!compiled)
        return compiled;

    m_compiled.insert_or_assign(uri, *compiled);

    // Also cache under the schema's own $id if it differs from the registration URI
    const auto& schemaId{compiled->schemaId()};
    if (!schemaId.isEmpty() && schemaId != uri)
        m_compiled.insert_or_assign(schemaId, *compiled);

    return compiled;
}

std::optional<JSONSchema> SchemaRegistry::get(const QString& uri) const
{
    const auto it{m_compiled.find(uri)};
    if (it != m_compiled.end())
        return it->second;
    return std::nullopt;
}

JSONSchema::ParseResult SchemaRegistry::create(const QJsonValue& schema, SchemaOptions options)
{
    using json_query::literals::operator""_qt_s;

    // Tier 1: check if we already have this schema compiled by $id
    if (schema.isObject())
    {
        const auto id{schema.toObject().value(u"$id"_qt_s).toString()};
        if (!id.isEmpty())
        {
            const auto it{m_compiled.find(id)};
            if (it != m_compiled.end())
                return it->second;
        }
    }

    // Compile with caching fetcher (Tier 2)
    auto compiled{JSONSchema::create(schema, makeCachingFetcher(), options)};
    if (!compiled)
        return compiled;

    // Cache the result by $id for future Tier 1 hits
    const auto& schemaId{compiled->schemaId()};
    if (!schemaId.isEmpty())
        m_compiled.insert_or_assign(schemaId, *compiled);

    return compiled;
}

bool SchemaRegistry::remove(const QString& uri)
{
    const auto compiledErased{m_compiled.erase(uri) > 0};
    const auto docErased{m_documents.erase(uri) > 0};
    return compiledErased || docErased;
}

void SchemaRegistry::clear()
{
    m_compiled.clear();
    m_documents.clear();
}

std::size_t SchemaRegistry::compiledCount() const noexcept { return m_compiled.size(); }

std::size_t SchemaRegistry::documentCount() const noexcept { return m_documents.size(); }

bool SchemaRegistry::contains(const QString& uri) const { return m_compiled.contains(uri); }

bool SchemaRegistry::containsDocument(const QString& uri) const { return m_documents.contains(uri); }

SchemaFetcher SchemaRegistry::makeCachingFetcher()
{
    return [this](const QString& uri) -> std::optional<QJsonValue>
    {
        // Tier 2: check document cache
        if (const auto it{m_documents.find(uri)}; it != m_documents.end())
            return it->second;

        // Fall through to user fetcher
        if (!m_userFetcher)
            return std::nullopt;

        auto result{m_userFetcher(uri)};
        if (result)
            m_documents[uri] = *result;

        return result;
    };
}

} // namespace json_query::json_schema
