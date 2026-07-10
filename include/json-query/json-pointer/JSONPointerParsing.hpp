// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "JSONPointerError.hpp"
#include "json-query/utils/JSONError.hpp"

#include <QString>
#include <QStringView>
#include <QVector>
#include <cstddef>
#include <cstring>
#include <expected>
#include <limits>
#include <optional>
#include <vector>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_pointer
{
// RFC 6901 §4: a token is interpreted relative to the container it meets at
// evaluation time. `key` is therefore always populated (also for Index tokens,
// where it holds the decoded decimal string); `kind == Index` additionally
// records the numeric value usable as an array index.
struct Token
{
    enum class Kind : quint8
    {
        Key,
        Index
    };
    Kind      kind{};
    qsizetype index{};
    QString   key{};
};
} // namespace json_query::json_pointer

namespace json_query::inline JSON_QUERY_ABI_NS::json_pointer::detail
{

// The complete RFC 6901 syntax check, shared between parsePointer (runtime)
// and the consteval-validated pointer literals (JSONPointerLiterals.hpp) —
// single source of truth so the two can never drift. After the
// container-relative token semantics (ADR-007) the grammar has exactly two
// error conditions: a non-empty pointer must start with '/', and '~' must be
// followed by '0' or '1'. Works on char (UTF-8) and char16_t: multi-byte/
// surrogate units never compare equal to the ASCII characters tested.
template <class CharT>
[[nodiscard]] constexpr std::optional<ParseError> validatePointerSyntax(const CharT* data, std::size_t size) noexcept
{
    if (size == 0)
        return std::nullopt; // "" addresses the root
    if (data[0] != CharT('/'))
        return ParseError::MissingLeadingSlash;
    for (std::size_t i = 1; i < size; ++i)
    {
        if (data[i] != CharT('~'))
            continue;
        if (i + 1 >= size || (data[i + 1] != CharT('0') && data[i + 1] != CharT('1')))
            return ParseError::InvalidEscapeSequence;
        ++i; // skip the escape's second character
    }
    return std::nullopt;
}

[[nodiscard]] inline QString decodeToken(QStringView token) noexcept
{
    const auto      len{token.size()};
    const char16_t* src{token.utf16()};
    auto            firstTilde{0};
    while (firstTilde < len && src[firstTilde] != u'~')
        ++firstTilde;
    if (firstTilde == len)
        return token.toString();

    QString out(len, Qt::Uninitialized);
    QChar*  dst{out.data()};
    std::memcpy(dst, src, firstTilde * sizeof(char16_t));
    auto wr{firstTilde};
    auto rd{firstTilde};
    while (rd < len)
    {
        const char16_t ch = src[rd++];
        if (ch == u'~' && rd < len)
        {
            switch (src[rd])
            {
            case u'0':
                dst[wr++] = u'~';
                ++rd;
                continue;
            case u'1':
                dst[wr++] = u'/';
                ++rd;
                continue;
            default:
                break;
            }
        }
        dst[wr++] = ch;
    }
    out.truncate(wr);
    return out;
}

[[nodiscard]] inline bool parseArrayIndex(const QString& s, qsizetype& out)
{
    if (s.isEmpty())
        return false;
    if (s == QLatin1String("0"))
    {
        out = 0;
        return true;
    }
    // RFC 6901: Leading zeros are not allowed in array indices
    if (s.startsWith(u'0'))
        return false;
    bool       ok;
    const auto value{s.toLongLong(&ok)};
    if (!ok || value < 0)
        return false;
    out = value;
    return true;
}

// Parse a JSON Pointer string into a sequence of tokens
[[nodiscard]] inline std::expected<void, ParseError> parsePointer(QStringView ptr, std::vector<Token>& tokens) noexcept
{
    tokens.clear();

    // Complete syntax check up front (shared with the compile-time literal
    // validation); the tokenization below can assume well-formed input.
    if (const auto err{validatePointerSyntax(ptr.utf16(), static_cast<std::size_t>(ptr.size()))})
        return std::unexpected(*err);

    constexpr char16_t Slash{u'/'};
    if (ptr.isEmpty())
        return {}; // success (empty pointer is valid)

    if (ptr.size() == 1)
    {
        tokens.push_back(Token{Token::Kind::Key, 0, QString{}});
        return {};
    }

    const auto approx{ptr.count(Slash)};
    tokens.reserve(approx);

    for (qsizetype begin{1};;)
    {
        const auto end{ptr.indexOf(Slash, begin)};
        const auto atEnd{end == -1};
        const auto raw{atEnd ? ptr.sliced(begin) : ptr.sliced(begin, end - begin)};

        const auto decoded{decodeToken(raw)};

        // Tokens that do not form a valid array index (leading zeros, overflow,
        // non-digits) are still valid pointers — they simply cannot step into an
        // array (RFC 6901 §4 interprets tokens per the container they meet).
        qsizetype idx{};
        if (parseArrayIndex(decoded, idx))
            tokens.push_back(Token{Token::Kind::Index, idx, decoded});
        else
            tokens.push_back(Token{Token::Kind::Key, 0, decoded});

        if (atEnd)
            break;

        begin = end + 1;
    }

    return {};
}

} // namespace json_query::json_pointer::detail
