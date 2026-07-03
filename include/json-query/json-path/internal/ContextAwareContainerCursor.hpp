// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "ContainerCursor.hpp"
#include "PathEvalCtx.hpp"

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include <functional>
#include <utility>
#include <iterator>
#include <compare>
#include <concepts>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path::internal
{

// ── Context Provider Concept ─────────────────────────────────────────

/**
 * @brief Concept defining requirements for context providers
 *
 * A context provider must provide access to root document and current context
 * through constexpr noexcept member functions. This enables compile-time
 * verification and zero-cost abstraction.
 */
template <typename T>
concept ContextProvider = requires(const T& provider) {
    { provider.rootDocument() } -> std::convertible_to<const QJsonValue&>;
    { provider.currentContext() } -> std::convertible_to<const QJsonValue&>;
} && std::is_nothrow_copy_constructible_v<T> && std::is_nothrow_move_constructible_v<T>;

// ── Context Provider Implementations ─────────────────────────────────

/**
 * @brief PathEvalCtx-based context provider
 *
 * Provides context access using the standard PathEvalCtx structure,
 * enabling seamless integration with existing JSONPath evaluation logic.
 */
class PathEvalContextProvider
{
  private:
    const detail::PathEvalCtx* ctx_;
    const QJsonValue*          currentContext_;

  public:
    constexpr PathEvalContextProvider(const detail::PathEvalCtx& ctx, const QJsonValue& current) noexcept
        : ctx_(&ctx), currentContext_(&current)
    {
    }

    [[nodiscard]] constexpr const QJsonValue& rootDocument() const noexcept { return ctx_->rootDocument; }

    [[nodiscard]] constexpr const QJsonValue& currentContext() const noexcept { return *currentContext_; }

    [[nodiscard]] constexpr const detail::PathEvalCtx& evalContext() const noexcept { return *ctx_; }
};

/**
 * @brief Simple context provider holding root and current context values
 *
 * Lightweight context provider for scenarios where only root and current
 * context access is needed without full PathEvalCtx.
 */
class SimpleContextProvider
{
  private:
    const QJsonValue* root_;
    const QJsonValue* current_;

  public:
    constexpr SimpleContextProvider(const QJsonValue& root, const QJsonValue& current) noexcept
        : root_(&root), current_(&current)
    {
    }

    [[nodiscard]] constexpr const QJsonValue& rootDocument() const noexcept { return *root_; }

    [[nodiscard]] constexpr const QJsonValue& currentContext() const noexcept { return *current_; }
};

/**
 * @brief Lambda-based context provider for functional context access
 */
template <typename RootFunc, typename CurrentFunc>
class LambdaContextProvider
{
  private:
    RootFunc    rootFn_;
    CurrentFunc currentFn_;

  public:
    constexpr LambdaContextProvider(RootFunc rootFn, CurrentFunc currentFn) noexcept
        : rootFn_(std::move(rootFn)), currentFn_(std::move(currentFn))
    {
    }

    [[nodiscard]] constexpr const QJsonValue& rootDocument() const noexcept { return rootFn_(); }

    [[nodiscard]] constexpr const QJsonValue& currentContext() const noexcept { return currentFn_(); }
};

// Verify our implementations satisfy the concept
static_assert(ContextProvider<PathEvalContextProvider>);
static_assert(ContextProvider<SimpleContextProvider>);

// ── Context-Aware ContainerCursor ─────────────────────────────────────

/**
 * @brief Context-aware ContainerCursor using concepts for type safety
 *
 * Extends ContainerCursor functionality to provide both zero-copy iteration
 * and seamless access to root document and evaluation context. Uses C++20/C++23
 * concepts to ensure type safety and provide excellent compile-time error messages.
 *
 * Features:
 * - All ContainerCursor performance benefits (32-byte, cache-aligned, zero-copy)
 * - Concept-based context access (no virtual functions, no template bloat)
 * - STL-compatible iterator with context-aware dereferencing
 * - C++23 constexpr support where possible
 * - Compatible with range-based for loops and STL algorithms
 * - Zero runtime overhead for context access
 * - Excellent compile-time error messages via concepts
 */
template <ContextProvider Provider>
class ContextAwareContainerCursor
{
  private:
    ContainerCursor cursor_;
    Provider        provider_;

  public:
    // STL-compatible type aliases
    using value_type            = std::pair<QJsonValue, const Provider&>;
    using size_type             = qsizetype;
    using difference_type       = qsizetype;
    using context_provider_type = Provider;

    /**
     * @brief Context-aware iterator providing both value and context access
     */
    class iterator
    {
      private:
        ContainerCursor::iterator base_;
        const Provider*           provider_;

      public:
        // C++23 iterator traits and concepts
        using iterator_concept  = std::forward_iterator_tag;
        using iterator_category = std::forward_iterator_tag;
        using value_type        = ContextAwareContainerCursor::value_type;
        using difference_type   = qsizetype;
        using pointer           = const value_type*;
        using reference         = value_type;

        constexpr iterator() noexcept = default;
        constexpr iterator(ContainerCursor::iterator base, const Provider* provider) noexcept
            : base_(base), provider_(provider)
        {
        }

        /// Dereference to get both JSON value and context access
        [[nodiscard]] value_type operator*() const noexcept { return {*base_, *provider_}; }

        /// Get just the JSON value (for compatibility with existing code)
        [[nodiscard]] constexpr QJsonValue value() const noexcept { return *base_; }

        /// Get context provider for root/context access (zero-cost)
        [[nodiscard]] constexpr const Provider& context() const noexcept { return *provider_; }

        /// Direct access to root document (zero-cost inline via concept)
        [[nodiscard]] constexpr const QJsonValue& rootDocument() const noexcept { return provider_->rootDocument(); }

        /// Direct access to current context (zero-cost inline via concept)
        [[nodiscard]] constexpr const QJsonValue& currentContext() const noexcept
        {
            return provider_->currentContext();
        }

        // Iterator advancement
        constexpr iterator& operator++() noexcept
        {
            ++base_;
            return *this;
        }

        constexpr iterator operator++(int) noexcept
        {
            auto tmp{*this};
            ++base_;
            return tmp;
        }

        // C++23 three-way comparison
        [[nodiscard]] constexpr auto operator<=>(const iterator& other) const noexcept
        {
            return base_ <=> other.base_;
        }

        [[nodiscard]] constexpr bool operator==(const iterator& other) const noexcept = default;
    };

    using const_iterator = iterator;

    // Construction
    constexpr ContextAwareContainerCursor() noexcept = default;

    // ContainerCursor is alignas(32); by-reference avoids gcc's -Wpsabi note
    // about the pre-4.6 parameter-passing ABI for over-aligned by-value types.
    constexpr ContextAwareContainerCursor(const ContainerCursor& cursor, Provider provider) noexcept
        : cursor_(cursor), provider_(std::move(provider))
    {
    }

    // Factory methods for common use cases
    [[nodiscard]] static constexpr ContextAwareContainerCursor object(const QJsonObject& obj,
                                                                      Provider           provider) noexcept
    {
        return {ContainerCursor::object(obj), std::move(provider)};
    }

    [[nodiscard]] static constexpr ContextAwareContainerCursor array(const QJsonArray& arr, Provider provider) noexcept
    {
        return {ContainerCursor::array(arr), std::move(provider)};
    }

    // STL-compatible container interface
    [[nodiscard]] constexpr iterator begin() const noexcept { return iterator{cursor_.begin(), &provider_}; }

    [[nodiscard]] constexpr iterator end() const noexcept { return iterator{cursor_.end(), &provider_}; }

    [[nodiscard]] constexpr iterator cbegin() const noexcept { return begin(); }
    [[nodiscard]] constexpr iterator cend() const noexcept { return end(); }

    // Container properties
    [[nodiscard]] constexpr bool empty() const noexcept { return cursor_.empty(); }

    [[nodiscard]] constexpr size_type size() const noexcept { return cursor_.size(); }

    // Access to underlying cursor and context
    [[nodiscard]] constexpr const ContainerCursor& cursor() const noexcept { return cursor_; }

    [[nodiscard]] constexpr const Provider& contextProvider() const noexcept { return provider_; }

    // Direct context access methods (zero-cost inline via concept)
    [[nodiscard]] constexpr const QJsonValue& rootDocument() const noexcept { return provider_.rootDocument(); }

    [[nodiscard]] constexpr const QJsonValue& currentContext() const noexcept { return provider_.currentContext(); }
};

// ── Type Aliases for Common Use Cases ─────────────────────────────────

/// Context-aware cursor using PathEvalCtx
using PathEvalContextCursor = ContextAwareContainerCursor<PathEvalContextProvider>;

/// Context-aware cursor using simple root + current context
using SimpleContextCursor = ContextAwareContainerCursor<SimpleContextProvider>;

// ── Factory Functions ─────────────────────────────────────────────────

/**
 * @brief Create a context-aware cursor from PathEvalCtx
 */
[[nodiscard]] constexpr PathEvalContextCursor
makeContextAwareCursor(const QJsonObject& obj, const detail::PathEvalCtx& ctx, const QJsonValue& current) noexcept
{
    return PathEvalContextCursor::object(obj, PathEvalContextProvider{ctx, current});
}

[[nodiscard]] constexpr PathEvalContextCursor
makeContextAwareCursor(const QJsonArray& arr, const detail::PathEvalCtx& ctx, const QJsonValue& current) noexcept
{
    return PathEvalContextCursor::array(arr, PathEvalContextProvider{ctx, current});
}

/**
 * @brief Create a simple context-aware cursor with root and current values
 */
[[nodiscard]] constexpr SimpleContextCursor
makeSimpleContextCursor(const QJsonObject& obj, const QJsonValue& root, const QJsonValue& current) noexcept
{
    return SimpleContextCursor::object(obj, SimpleContextProvider{root, current});
}

[[nodiscard]] constexpr SimpleContextCursor
makeSimpleContextCursor(const QJsonArray& arr, const QJsonValue& root, const QJsonValue& current) noexcept
{
    return SimpleContextCursor::array(arr, SimpleContextProvider{root, current});
}

/**
 * @brief Create a lambda-based context cursor
 */
template <typename RootFunc, typename CurrentFunc>
[[nodiscard]] constexpr auto
makeLambdaContextCursor(const QJsonObject& obj, RootFunc&& rootFn, CurrentFunc&& currentFn) noexcept
{
    using Provider = LambdaContextProvider<std::decay_t<RootFunc>, std::decay_t<CurrentFunc>>;
    return ContextAwareContainerCursor<Provider>::object(
        obj, Provider{std::forward<RootFunc>(rootFn), std::forward<CurrentFunc>(currentFn)});
}

template <typename RootFunc, typename CurrentFunc>
[[nodiscard]] constexpr auto
makeLambdaContextCursor(const QJsonArray& arr, RootFunc&& rootFn, CurrentFunc&& currentFn) noexcept
{
    using Provider = LambdaContextProvider<std::decay_t<RootFunc>, std::decay_t<CurrentFunc>>;
    return ContextAwareContainerCursor<Provider>::array(
        arr, Provider{std::forward<RootFunc>(rootFn), std::forward<CurrentFunc>(currentFn)});
}

/**
 * @brief Generic factory function that accepts any ContextProvider
 */
template <ContextProvider Provider>
[[nodiscard]] constexpr ContextAwareContainerCursor<Provider> makeContextCursor(const QJsonObject& obj,
                                                                                Provider           provider) noexcept
{
    return ContextAwareContainerCursor<Provider>::object(obj, std::move(provider));
}

template <ContextProvider Provider>
[[nodiscard]] constexpr ContextAwareContainerCursor<Provider> makeContextCursor(const QJsonArray& arr,
                                                                                Provider          provider) noexcept
{
    return ContextAwareContainerCursor<Provider>::array(arr, std::move(provider));
}

// ── ADL Support ─────────────────────────────────────────────────────

template <ContextProvider Provider>
[[nodiscard]] constexpr auto begin(const ContextAwareContainerCursor<Provider>& cursor) noexcept
{
    return cursor.begin();
}

template <ContextProvider Provider>
[[nodiscard]] constexpr auto end(const ContextAwareContainerCursor<Provider>& cursor) noexcept
{
    return cursor.end();
}

} // namespace json_query::inline JSON_QUERY_ABI_NS::json_path::internal
