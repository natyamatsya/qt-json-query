// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

/**
 * @brief Brace-init-safe wrapper for Qt JSON containers
 *
 * Prevents the QJsonArray brace-initialization footgun where
 * `QJsonArray{existingArray}` triggers the `initializer_list<QJsonValue>`
 * constructor instead of the copy constructor, silently wrapping the array
 * inside another array.
 *
 * By returning BraceSafe<QJsonArray> from a factory function, `auto` deduces
 * this wrapper type instead of QJsonArray, so brace-init is safe:
 *
 *   const auto arr{asArray(value)};       // OK — BraceSafe, not QJsonArray
 *   for (const auto& item : arr) { ... }  // OK — range support
 *   arr.size();                            // OK — forwarded
 *   arr[0];                                // OK — forwarded
 *   someFunc(arr.get());                   // OK — explicit unwrap
 *
 * See ADR-001: doc/adr/001-no-brace-init-for-qt-json-containers.md
 */

namespace json_query
{

template <typename T>
struct BraceSafe
{
    T value;

    // Range support
    [[nodiscard]] auto begin() const { return value.begin(); }
    [[nodiscard]] auto end() const { return value.end(); }

    // Size / emptiness
    [[nodiscard]] auto size() const { return value.size(); }
    [[nodiscard]] bool isEmpty() const { return value.isEmpty(); }

    // Element access
    [[nodiscard]] auto operator[](auto&& key) const { return value[std::forward<decltype(key)>(key)]; }
    [[nodiscard]] auto at(auto&& key) const { return value.at(std::forward<decltype(key)>(key)); }

    // Key lookup (QJsonObject)
    [[nodiscard]] bool contains(auto&& key) const { return value.contains(std::forward<decltype(key)>(key)); }
    [[nodiscard]] auto find(auto&& key) const { return value.find(std::forward<decltype(key)>(key)); }

    // Explicit unwrap
    [[nodiscard]] const T& get() const& { return value; }
    [[nodiscard]] T&&      get() && { return std::move(value); }

    // Implicit conversion for passing to functions expecting the container
    [[nodiscard]] operator const T&() const& { return value; }
    [[nodiscard]] operator T() && { return std::move(value); }
};

// Factory functions

[[nodiscard]] inline BraceSafe<QJsonArray> asArray(const QJsonValue& v) { return {v.toArray()}; }

[[nodiscard]] inline BraceSafe<QJsonObject> asObject(const QJsonValue& v) { return {v.toObject()}; }

} // namespace json_query
