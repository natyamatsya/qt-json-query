// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include <QString>
#include <QStringView>
#include <cstddef>

#include "json-query/json-pointer/JSONPointer.hpp"
#include "json-query/json-pointer/JSONPointerParsing.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_pointer
{

namespace detail
{

// Not defined: referencing it in a constant-evaluated context makes the
// compile error name the actual problem.
void invalid_rfc6901_pointer_literal();

// NTTP carrier for _jptr literals; the consteval constructor runs the SAME
// syntax validation as parsePointer (validatePointerSyntax — single source
// of truth), so an invalid literal is a compile error.
template <class CharT, std::size_t N>
struct PointerLiteral
{
    CharT value[N]{};

    consteval PointerLiteral(const CharT (&str)[N])
    {
        for (std::size_t i = 0; i < N; ++i)
            value[i] = str[i];
        if (validatePointerSyntax(str, N - 1)) // N includes the terminator
            invalid_rfc6901_pointer_literal();
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return N - 1; }
};

template <PointerLiteral Lit>
[[nodiscard]] inline const JSONPointer& compileLiteral()
{
    // Compile-once per literal (magic static); create() cannot fail — the
    // syntax was proven in the consteval constructor above.
    static const JSONPointer compiled = []
    {
        QString text;
        if constexpr (sizeof(Lit.value[0]) == 1)
            text = QString::fromUtf8(reinterpret_cast<const char*>(Lit.value), static_cast<qsizetype>(Lit.size()));
        else
            text = QString::fromUtf16(Lit.value, static_cast<qsizetype>(Lit.size()));
        return JSONPointer::create(text).value();
    }();
    return compiled;
}

} // namespace detail

/**
 * @brief Compile-time-validated JSON Pointer literals.
 *
 * Opt in with `using namespace json_query::literals;`. The literal's
 * RFC 6901 syntax is checked at compile time (an invalid literal fails to
 * compile), so the result needs no std::expected unwrapping — the
 * assert-and-unwrap helpers around create() for known-good literals become
 * unnecessary. Each distinct literal is compiled once (function-local
 * static) and returned by reference.
 *
 * @code
 * using namespace json_query::literals;
 *
 * const auto& name{"/data/attributes/name"_jptr}; // no .value() ceremony
 * auto value{name.evaluate(doc)};
 *
 * // "/data/attributes/näme"_jptr  — UTF-8 keys work
 * // "data/name"_jptr              — compile error: missing leading slash
 * // "/a~2b"_jptr                  — compile error: invalid escape
 * @endcode
 */
namespace literals
{

template <detail::PointerLiteral Lit>
[[nodiscard]] inline const JSONPointer& operator""_jptr()
{
    return detail::compileLiteral<Lit>();
}

} // namespace literals

} // namespace json_query::json_pointer
