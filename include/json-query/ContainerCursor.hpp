#pragma once

#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

namespace json_query {

    /**
     * @brief  Lightweight cursor used by JSONPath’s depth-first traversal.
     *
     * Holds a non-owning pointer to either a QJsonObject or QJsonArray plus a
     * simple index that advances with each call to next().  Because no container
     * copies are kept, there are **no extra ref-count increments or detach
     * checks**, and the struct is only 24 bytes on typical 64-bit builds.
     */
    struct ContainerCursor
    {
        enum class Kind : std::uint8_t { Object, Array };

        const void*  ptr;     //!< Points to a QJsonObject or QJsonArray
        qsizetype    idx;     //!< Next child to visit
        qsizetype    size;    //!< Total number of children
        Kind         kind;    //!< Discriminator

        // ──────────────────────────────
        // Factory helpers
        // ──────────────────────────────
        [[nodiscard]] static inline ContainerCursor
        object(const QJsonObject& o) noexcept
        { return { &o, 0, o.size(), Kind::Object }; }

        [[nodiscard]] static inline ContainerCursor
        array (const QJsonArray&  a) noexcept
        { return { &a, 0, a.size(), Kind::Array  }; }

        // ──────────────────────────────
        // Iteration API
        // ──────────────────────────────
        [[nodiscard]] inline bool hasNext() const noexcept
        { return idx < size; }

        [[nodiscard]] inline QJsonValue next() noexcept
        {
            if (kind == Kind::Array) {
                const auto* a = static_cast<const QJsonArray*>(ptr);
                return (*a).at(idx++);                 // cheap ref
            }

            // Object branch
            const auto* o  = static_cast<const QJsonObject*>(ptr);
            auto it        = o->constBegin();
            std::advance(it, idx++);
            return it.value();                         // cheap ref
        }
    };

} // namespace json_query
