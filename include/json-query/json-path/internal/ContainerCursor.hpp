#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include <cstdint>
#include <cstddef>
#include <iterator>
#include <compare>

// Opt-in ranges support - define JSON_QUERY_ENABLE_RANGES to enable
#ifdef JSON_QUERY_ENABLE_RANGES
#include <ranges>
#endif

namespace json_query::json_path::internal {

    /**
     * @brief  Modern C++23 STL-compatible cursor for JSONPath depth-first traversal.
     *
     * Provides a lightweight, cache-aligned iterator interface over QJsonObject or QJsonArray
     * with zero-copy semantics and no extra ref-count increments or detach checks.
     * 
     * Features:
     * - STL-compatible iterator interface with begin()/end()
     * - C++23 constexpr support where possible
     * - Three-way comparison operators
     * - Range-based for loop compatibility
     * - Optional C++23 ranges support (opt-in via JSON_QUERY_ENABLE_RANGES)
     *
     * @remark  32‑byte‑aligned, 32‑byte‑sized cursor for optimal cache performance.
     *
     * Uses 1‑bit‑stealing tagged pointer to discriminate object vs array,
     * plus two qsizetypes for index/size, and 8 bytes of padding to reach 32 bytes.
     */
    class alignas(32) ContainerCursor
    {
        // Tagged pointer implementation - steal the low bit as a type tag
        static constexpr std::uintptr_t TAG_MASK  = 0x1;
        static constexpr std::uintptr_t ARRAY_TAG = 0x1;

        static constexpr const QJsonObject* objPtr(std::uintptr_t p) noexcept {
            return reinterpret_cast<const QJsonObject*>(p & ~TAG_MASK);
        }
        static constexpr const QJsonArray* arrPtr(std::uintptr_t p) noexcept {
            return reinterpret_cast<const QJsonArray*>(p & ~TAG_MASK);
        }

    public:
        // ── STL container type aliases ──────────────────────────────────────
        class iterator;
        using value_type = QJsonValue;
        using size_type = qsizetype;
        using difference_type = qsizetype;
        using const_iterator = iterator;  // read-only container

        // ── Factory methods ─────────────────────────────────────────────────
        [[nodiscard]] static constexpr ContainerCursor
        object(const QJsonObject& o) noexcept {
            return { reinterpret_cast<std::uintptr_t>(&o), o.size() };
        }

        [[nodiscard]] static constexpr ContainerCursor
        array(const QJsonArray& a) noexcept {
            return { reinterpret_cast<std::uintptr_t>(&a) | ARRAY_TAG, a.size() };
        }

        // ── STL container interface ─────────────────────────────────────────
        [[nodiscard]] constexpr iterator begin() const noexcept;
        [[nodiscard]] constexpr iterator end() const noexcept;
        [[nodiscard]] constexpr iterator cbegin() const noexcept { return begin(); }
        [[nodiscard]] constexpr iterator cend() const noexcept { return end(); }

        // ── Container properties ────────────────────────────────────────────
        [[nodiscard]] constexpr bool empty() const noexcept {
            return m_size == 0;
        }

        [[nodiscard]] constexpr size_type size() const noexcept {
            return m_size;
        }

        // ── Legacy compatibility (deprecated) ───────────────────────────────
        [[deprecated("Use range-based for loop or iterators instead")]]
        [[nodiscard]] bool hasNext() const noexcept {
            return m_idx < m_size;
        }

        [[deprecated("Use range-based for loop or iterators instead")]]
        [[nodiscard]] QJsonValue next() const noexcept {
            if (m_tagged & ARRAY_TAG) {
                const auto* a = arrPtr(m_tagged);
                return a->at(m_idx++);
            } else {
                const auto* o = objPtr(m_tagged);
                auto it = o->constBegin();
                std::advance(it, m_idx++);
                return it.value();
            }
        }

        // ── STL-compatible iterator implementation ──────────────────────────
        class iterator {
        public:
            // C++23 iterator traits and concepts
            using iterator_concept = std::forward_iterator_tag;
            using iterator_category = std::forward_iterator_tag;
            using value_type = QJsonValue;
            using difference_type = qsizetype;
            using pointer = const QJsonValue*;
            using reference = const QJsonValue&;

            // ── Constructor ──────────────────────────────────────────────────
            constexpr iterator() noexcept = default;
            
            constexpr iterator(std::uintptr_t tagged, qsizetype idx, qsizetype size) noexcept
                : m_tagged(tagged), m_idx(idx), m_size(size) {}

            // ── Iterator operations ─────────────────────────────────────────
            [[nodiscard]] QJsonValue operator*() const noexcept {
                if (m_tagged & ARRAY_TAG) {
                    const auto* a = arrPtr(m_tagged);
                    return a->at(m_idx);
                } else {
                    const auto* o = objPtr(m_tagged);
                    auto it = o->constBegin();
                    std::advance(it, m_idx);
                    return it.value();
                }
            }

            constexpr iterator& operator++() noexcept {
                ++m_idx;
                return *this;
            }

            constexpr iterator operator++(int) noexcept {
                iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            // ── C++23 three-way comparison ──────────────────────────────────
            [[nodiscard]] constexpr auto operator<=>(const iterator& other) const noexcept {
                if (auto cmp = m_tagged <=> other.m_tagged; cmp != 0) return cmp;
                return m_idx <=> other.m_idx;
            }

            [[nodiscard]] constexpr bool operator==(const iterator& other) const noexcept {
                return m_tagged == other.m_tagged && m_idx == other.m_idx;
            }

#ifdef JSON_QUERY_ENABLE_RANGES
            // ── C++20/23 sentinel support ───────────────────────────────────
            [[nodiscard]] constexpr bool operator==(std::default_sentinel_t) const noexcept {
                return m_idx >= m_size;
            }
#endif

        private:
            std::uintptr_t m_tagged{0};
            qsizetype m_idx{0};
            qsizetype m_size{0};

            friend class ContainerCursor;
        };

    private:
        // Private constructor used only by factory methods
        constexpr ContainerCursor(std::uintptr_t tagged, qsizetype size) noexcept
            : m_tagged(tagged), m_size(size) {}

        std::uintptr_t m_tagged;   // ptr + 1‑bit tag        (8 B)
        mutable qsizetype m_idx{0}; // current position      (8 B) - mutable for legacy compatibility
        qsizetype      m_size;     // total children         (8 B)
        std::byte      m_pad[8]{}; // padding → struct is   (32 B)

        friend class iterator;
    };

    // ── Iterator implementation ─────────────────────────────────────────────
    constexpr ContainerCursor::iterator ContainerCursor::begin() const noexcept {
        return iterator(m_tagged, 0, m_size);
    }

    constexpr ContainerCursor::iterator ContainerCursor::end() const noexcept {
        return iterator(m_tagged, m_size, m_size);
    }

    // ── Free functions for ADL support ─────────────────────────────────────
    constexpr auto begin(const ContainerCursor& cursor) noexcept -> ContainerCursor::iterator {
        return cursor.begin();
    }

    constexpr auto end(const ContainerCursor& cursor) noexcept -> ContainerCursor::iterator {
        return cursor.end();
    }

    // Enforce our performance assumptions:
    static_assert(sizeof(ContainerCursor)  == 32, "Cursor must be 32-bytes");
    static_assert(alignof(ContainerCursor) == 32, "Cursor must be 32‑byte aligned");

    static_assert(alignof(QJsonObject) % 2 == 0, "Pointer LSB is available");
    static_assert(alignof(QJsonArray ) % 2 == 0, "Pointer LSB is available");

} // namespace json_query::json_path::internal

#ifdef JSON_QUERY_ENABLE_RANGES
// ── Optional C++23 ranges support ──────────────────────────────────────────

namespace std::ranges {
    // Enable borrowed_range for ContainerCursor (safe to iterate after container destruction)
    template<>
    inline constexpr bool enable_borrowed_range<json_query::json_path::internal::ContainerCursor> = true;
}

namespace json_query::ranges {
    /**
     * @brief Custom range view for JSON container traversal
     * 
     * Provides a composable view over JSON containers that works with std::ranges.
     * Only available when JSON_QUERY_ENABLE_RANGES is defined.
     */
    class json_values_view : public std::ranges::view_interface<json_values_view> {
        json_path::internal::ContainerCursor cursor_;
    public:
        constexpr json_values_view(json_path::internal::ContainerCursor cursor) noexcept 
            : cursor_(cursor) {}
        
        constexpr auto begin() const noexcept { return cursor_.begin(); }
        constexpr auto end() const noexcept { return cursor_.end(); }
        constexpr bool empty() const noexcept { return cursor_.empty(); }
        constexpr auto size() const noexcept { return cursor_.size(); }
    };

    /**
     * @brief Range adaptor for JSON containers
     * 
     * Automatically creates appropriate ContainerCursor for QJsonObject or QJsonArray.
     * Usage: jsonObj | json_query::ranges::json_values
     */
    inline constexpr auto json_values = []<typename T>(T&& container) {
        if constexpr (std::same_as<std::decay_t<T>, QJsonObject>) {
            return json_values_view{json_path::internal::ContainerCursor::object(container)};
        } else {
            static_assert(std::same_as<std::decay_t<T>, QJsonArray>, 
                         "json_values only supports QJsonObject and QJsonArray");
            return json_values_view{json_path::internal::ContainerCursor::array(container)};
        }
    };
}

#endif // JSON_QUERY_ENABLE_RANGES
