// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include "JSONPointerError.hpp"
#include "json-query/utils/JSONError.hpp"

#include <QString>
#include <QStringView>
#include <QVector>
#include <expected>
#include <limits>
#include <cstring>
#include <vector>

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_pointer
{
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
    using enum ParseError;

    tokens.clear();
    constexpr char16_t Slash{u'/'};
    if (ptr.isEmpty())
        return {}; // success (empty pointer is valid)

    if (ptr.front() != Slash)
        return std::unexpected(MissingLeadingSlash);

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
        if (raw.contains(u'~') && decoded.isEmpty() && !raw.isEmpty())
        {
            // decodeToken failing would produce same string; we approximate by checking unsupported escape later
        }

        qsizetype idx{};
        if (parseArrayIndex(decoded, idx))
        {
            tokens.push_back(Token{Token::Kind::Index, idx, {}});
        }
        else
        {
            bool digits{!decoded.isEmpty()};
            for (QChar ch : decoded)
            {
                if (ch < u'0' || ch > u'9')
                {
                    digits = false;
                    break;
                }
            }
            if (digits)
                return std::unexpected(ArrayIndexOverflow);

            // Check for invalid escape sequences in the raw token
            for (qsizetype i = 0; i < raw.size(); ++i)
            {
                if (raw[i] == u'~')
                {
                    if (i + 1 >= raw.size())
                        return std::unexpected(InvalidEscapeSequence);
                    const auto next = raw[i + 1];
                    if (next != u'0' && next != u'1')
                        return std::unexpected(InvalidEscapeSequence);
                    ++i; // Skip the next character as it's part of the escape sequence
                }
            }

            if (decoded.isEmpty() && !raw.isEmpty())
                return std::unexpected(InvalidEscapeSequence);

            tokens.push_back(Token{Token::Kind::Key, 0, decoded});
        }

        if (atEnd)
            break;

        begin = end + 1;
    }

    return {};
}

} // namespace json_query::json_pointer::detail
