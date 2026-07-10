// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include <QString>
#include <QStringView>
#include <cstddef>

#include "json-query/json-path/JSONPath.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path
{

namespace detail
{

// Not defined: referencing it in a constant-evaluated context makes the
// compile error name the actual problem.
void jsonpath_literal_must_start_with_root_identifier();

// The only RFC 9535 rule that is checked at compile time: a query is
// root-identifier ("$") followed by segments (§2.2). Everything beyond that
// (selectors, filters, function well-typedness) is validated by the REAL
// parser at first use — a second, constexpr grammar would inevitably drift
// from JSONPath::create (the divergence class ADR-007 forbids), which is why
// _jpath is compile-ONCE, not compile-time-validated like _jptr.
template <class CharT>
[[nodiscard]] constexpr bool startsWithRootIdentifier(const CharT* data, std::size_t size) noexcept
{
    return size > 0 && data[0] == CharT('$');
}

// NTTP carrier for _jpath literals
template <class CharT, std::size_t N>
struct PathLiteral
{
    CharT value[N]{};

    consteval PathLiteral(const CharT (&str)[N])
    {
        for (std::size_t i = 0; i < N; ++i)
            value[i] = str[i];
        if (!startsWithRootIdentifier(str, N - 1)) // N includes the terminator
            jsonpath_literal_must_start_with_root_identifier();
    }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return N - 1; }
};

template <PathLiteral Lit>
[[nodiscard]] inline const JSONPath& compilePathLiteral()
{
    // Compile-once per literal (magic static). An invalid literal is a
    // programming error and fails FAST and loud on first use — the same
    // fatal-on-impossible philosophy as the OOM policy; use create() for
    // queries that need recoverable error handling.
    static const JSONPath compiled = []
    {
        // char16_t literals view directly into the NTTP array; UTF-8 converts
        QString text;
        QStringView view;
        if constexpr (sizeof(Lit.value[0]) == 1)
        {
            text = QString::fromUtf8(reinterpret_cast<const char*>(Lit.value), static_cast<qsizetype>(Lit.size()));
            view = text;
        }
        else
        {
            view = QStringView{Lit.value, static_cast<qsizetype>(Lit.size())};
        }

        auto result{JSONPath::create(view)};
        if (!result)
            qFatal("Invalid JSONPath literal \"%s\": %s", qPrintable(view.toString()),
                   qPrintable(result.error().formatted_message()));
        return std::move(*result);
    }();
    return compiled;
}

} // namespace detail

/**
 * @brief Compile-once JSONPath literals.
 *
 * Opt in with `using namespace json_query::literals;`. Unlike "_jptr"
 * (whose full RFC 6901 syntax is checked at compile time), a "_jpath"
 * literal is validated by the real RFC 9535 parser at FIRST USE: only the
 * root-identifier rule is a compile error; any other syntax error in the
 * literal is a fatal runtime error (programming error, not a recoverable
 * condition — use JSONPath::create for runtime-provided queries). Each
 * distinct literal is compiled once (function-local static) and returned by
 * reference, replacing per-call-site assert-and-unwrap helpers.
 *
 * @code
 * using namespace json_query::literals;
 *
 * const auto& users{"$[?(@.type=='user')]"_jpath}; // no .value() ceremony
 * auto nodes{users.evaluate(doc)};
 *
 * // "[?(@.x)]"_jpath — compile error: missing root identifier '$'
 * // "$[?(@.x"_jpath  — fatal at first use: invalid RFC 9535 syntax
 * @endcode
 */
namespace literals
{

template <detail::PathLiteral Lit>
[[nodiscard]] inline const JSONPath& operator""_jpath()
{
    return detail::compilePathLiteral<Lit>();
}

} // namespace literals

} // namespace json_query::json_path
