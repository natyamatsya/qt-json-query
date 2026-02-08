// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
#pragma once

#include <QtCore/QJsonValue>
#include <QtCore/QString>

#include <optional>
#include <unordered_map>

#include "JSONSchema.hpp"

namespace json_query::json_schema
{

/**
 * @brief Two-tier caching layer for compiled JSON Schemas
 *
 * SchemaRegistry provides:
 *
 * **Tier 1 — Compiled schema cache**: Avoids re-compilation of previously
 * seen schemas. Schemas are cached by their canonical URI ($id). Since
 * JSONSchema holds a shared_ptr<const CompiledSchema>, cache entries are
 * cheap reference-counted pointers.
 *
 * **Tier 2 — Fetch document cache**: Avoids re-fetching remote schemas
 * across compilations. When a schema $ref references a remote URI that was
 * already fetched (for any previous compilation), the cached raw JSON
 * document is reused, eliminating the I/O cost.
 *
 * Thread-safety: The registry itself is NOT thread-safe. External
 * synchronization is required for concurrent access. The returned
 * JSONSchema objects are immutable and safe to share across threads.
 *
 * @example
 * @code
 * SchemaRegistry registry;
 * registry.setFetcher([](const QString& uri) -> std::optional<QJsonValue> {
 *     return httpGet(uri);  // expensive I/O
 * });
 *
 * // First compilation: fetches remote refs, compiles, and caches
 * auto schema1 = registry.create(schemaJson1);
 *
 * // Second compilation referencing the same remote: no I/O, uses cached doc
 * auto schema2 = registry.create(schemaJson2);
 *
 * // Re-requesting the same schema by $id: instant shared_ptr copy
 * auto cached = registry.get("https://example.com/my-schema");
 * @endcode
 */
class SchemaRegistry
{
  public:
    SchemaRegistry()                                 = default;
    ~SchemaRegistry()                                = default;
    SchemaRegistry(SchemaRegistry&&) noexcept        = default;
    SchemaRegistry& operator=(SchemaRegistry&&)      = default;
    SchemaRegistry(const SchemaRegistry&)            = default;
    SchemaRegistry& operator=(const SchemaRegistry&) = default;

    /**
     * @brief Set the upstream fetcher for URIs not in the cache
     *
     * The registry wraps this fetcher with its document cache. Remote
     * documents fetched through this callback are cached for future use.
     */
    void setFetcher(SchemaFetcher fetcher);

    /**
     * @brief Pre-register a schema under a given URI
     *
     * Compiles the schema and caches it under @p uri. Also caches the
     * raw document so that other schemas referencing this URI via $ref
     * can resolve without fetching.
     *
     * @param uri  Canonical URI to register under
     * @param schema  The JSON Schema value (object or boolean)
     * @param options  Compilation options
     * @return The compiled schema, or a compilation error
     */
    JSONSchema::ParseResult add(const QString& uri, const QJsonValue& schema,
                                SchemaOptions options = {});

    /**
     * @brief Retrieve a previously compiled schema by URI
     *
     * @param uri  The $id or registration URI
     * @return The cached schema, or std::nullopt if not found
     */
    [[nodiscard]] std::optional<JSONSchema> get(const QString& uri) const;

    /**
     * @brief Compile a schema with registry-backed caching
     *
     * If the schema has a $id that is already cached, returns the cached
     * compiled schema immediately (Tier 1 hit). Otherwise, compiles the
     * schema using a caching fetcher that checks the document cache
     * (Tier 2) before falling through to the upstream fetcher.
     *
     * Successfully compiled schemas with a $id are automatically cached.
     *
     * @param schema  The JSON Schema value (object or boolean)
     * @param options Compilation options
     * @return The compiled schema, or a compilation error
     */
    JSONSchema::ParseResult create(const QJsonValue& schema, SchemaOptions options = {});

    /**
     * @brief Remove a cached schema by URI
     *
     * Removes from both the compiled schema cache and the document cache.
     *
     * @return true if something was removed
     */
    bool remove(const QString& uri);

    /**
     * @brief Clear all caches
     */
    void clear();

    /**
     * @brief Number of compiled schemas in the cache
     */
    [[nodiscard]] std::size_t compiledCount() const noexcept;

    /**
     * @brief Number of cached raw documents
     */
    [[nodiscard]] std::size_t documentCount() const noexcept;

    /**
     * @brief Check if a compiled schema is cached for the given URI
     */
    [[nodiscard]] bool contains(const QString& uri) const;

    /**
     * @brief Check if a raw document is cached for the given URI
     */
    [[nodiscard]] bool containsDocument(const QString& uri) const;

  private:
    /// Tier 1: compiled schema cache (URI -> JSONSchema)
    std::unordered_map<QString, JSONSchema> m_compiled{};

    /// Tier 2: raw document cache (URI -> QJsonValue)
    std::unordered_map<QString, QJsonValue> m_documents{};

    /// Upstream fetcher provided by the user
    SchemaFetcher m_userFetcher{};

    /**
     * @brief Build a caching fetcher that wraps the user fetcher
     *
     * Checks m_documents first, then falls through to m_userFetcher,
     * caching the result on success.
     */
    [[nodiscard]] SchemaFetcher makeCachingFetcher();
};

} // namespace json_query::json_schema
