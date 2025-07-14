#pragma once

// -----------------------------------------------------------------------------
//  qt_hash :  cast-to-32-bit wrapper around Qt's qHash()
//             • Works with any type Q that already has a qHash()
//               overload (with or without seed parameter).
//             • Returns quint32 — consistent width across 32/64-bit builds.
//             • Zero cost in release; just an inline cast & mask.
// -----------------------------------------------------------------------------
#include <QtGlobal>
#include <type_traits>
#include <concepts>

namespace detail {

    //  Detect whether qHash(T, uint) exists  (newer overloads)
    template<typename T>
    concept HashWithSeed = requires (const T& v) {
        { qHash(v, 0u) } -> std::convertible_to<qsizetype>;
    };

    //  Detect plain qHash(T)  (older overloads / fallback)
    template<typename T>
    concept HashPlain = requires (const T& v) {
        { qHash(v) } -> std::convertible_to<qsizetype>;
    };

} // namespace detail

template<typename T>
[[nodiscard]] constexpr quint32 qt_hash(const T& v) noexcept
{
    qsizetype raw;
    if constexpr (detail::HashWithSeed<T>)   raw = qHash(v, 0u);
    else                                     raw = qHash(v);

    return static_cast<quint32>(static_cast<std::uint64_t>(raw) & 0xFFFF'FFFFu);
}