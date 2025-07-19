#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include <cstdint>
#include <cstddef>

namespace json_query::json_path::internal {

    /**
     * @brief  Lightweight cursor used by JSONPath's depth-first traversal.
     *
     * Holds a non-owning pointer to either a QJsonObject or QJsonArray plus a
     * simple index that advances with each call to next().  Because no container
     * copies are kept, there are **no extra ref-count increments or detach
     * checks**, and the struct is only 24 bytes on typical 64-bit builds.
     *
     * @remark  32‑byte‑aligned, 32‑byte‑sized cursor for JSONPath traversal.
     *
     * Uses a 1‑bit‑stealing tagged pointer to discriminate object vs array,
     * plus two qsizetypes for index/size, and 8 bytes of padding to land at 32 œbytes.
     */
    class alignas(32) ContainerCursor
    {
        // steal the low bit of the pointer as a tag:
        static constexpr std::uintptr_t TAG_MASK  = 0x1;
        static constexpr std::uintptr_t ARRAY_TAG = 0x1;

        static const QJsonObject* objPtr(std::uintptr_t p) {
            return reinterpret_cast<const QJsonObject*>(p & ~TAG_MASK);
        }
        static const QJsonArray*  arrPtr(std::uintptr_t p) {
            return reinterpret_cast<const QJsonArray*>(p & ~TAG_MASK);
        }

        public:
        // ── factories ────────────────────────────────────────────────────────
        [[nodiscard]] static inline ContainerCursor
        object(const QJsonObject& o) noexcept {
            return { reinterpret_cast<std::uintptr_t>(&o), o.size() };
        }

        [[nodiscard]] static inline ContainerCursor
        array(const QJsonArray& a) noexcept {
            return { reinterpret_cast<std::uintptr_t>(&a) | ARRAY_TAG,
                     a.size() };
        }

        // ── iteration API ────────────────────────────────────────────────────
        [[nodiscard]] bool hasNext() const noexcept {
            return m_idx < m_size;
        }

        [[nodiscard]] QJsonValue next() noexcept {
            if (m_tagged & ARRAY_TAG) {
                const auto* a = arrPtr(m_tagged);
                return a->at(m_idx++);  // cheap ref
            } else {
                const auto* o = objPtr(m_tagged);
                auto it = o->constBegin();
                std::advance(it, m_idx++);
                return it.value();      // cheap ref
            }
        }

        private:
        // private ctor used only by the factories
        ContainerCursor(std::uintptr_t tagged, qsizetype n) noexcept
            : m_tagged(tagged), m_size(n) {}

        std::uintptr_t m_tagged;   // ptr + 1‑bit tag        (8 B)
        qsizetype      m_idx{0};   // next child             (8 B)
        qsizetype      m_size;     // total children         (8 B)
        std::byte      m_pad[8]{}; // padding → struct is   (32 B)

        // no extra implicit padding: total sizeof == 32
    };

    // Enforce our assumptions:
    static_assert(sizeof(ContainerCursor)  == 32, "Cursor must be 32-bytes");
    static_assert(alignof(ContainerCursor) == 32, "Cursor must be 32‑byte aligned");

    static_assert(alignof(QJsonObject) % 2 == 0, "Pointer LSB is available");
    static_assert(alignof(QJsonArray ) % 2 == 0, "Pointer LSB is available");
} // namespace json_query::json_path::internal
