#pragma once

#include <QString>
#include <QStringView>
#include <QVector>
#include <expected>
#include <limits>
#include <cstring>
#include <vector>

namespace json_query::json_pointer::detail
{

struct Token
{
    enum class Kind : quint8
    {
        Key,
        Index
    };
    Kind      kind;
    qsizetype index{};
    QString   key{};
};

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

[[nodiscard]] inline bool parseArrayIndex(const QString& s, qsizetype& out) noexcept
{
    if (s.isEmpty())
        return false;
    qsizetype value{0};
    for (const QChar ch : s)
    {
        if (ch < u'0' || ch > u'9')
            return false;
        const auto digit{ch.unicode() - u'0'};
        if (value > (std::numeric_limits<qsizetype>::max() - digit) / 10)
            return false;
        value = value * 10 + digit;
    }
    out = value;
    return true;
}

enum class ParseError : std::uint8_t
{
    MissingLeadingSlash,
    EmptyNonTerminalToken,
    InvalidEscapeSequence,
    NonDecimalArrayIndex,
    ArrayIndexOverflow
};

[[nodiscard]] inline std::expected<void, ParseError> parsePointer(QStringView ptr, std::vector<Token>& tokens) noexcept
{
    tokens.clear();
    constexpr char16_t Slash{u'/'};
    if (ptr.isEmpty())
        return std::expected<void, ParseError>{}; // success
    if (ptr.front() != Slash)
        return std::unexpected(ParseError::MissingLeadingSlash);
    if (ptr.size() == 1)
    {
        tokens.push_back(Token{Token::Kind::Key, 0, QString{}});
        return std::expected<void, ParseError>{};
    }

    const auto approx{ptr.count(Slash)};
    tokens.reserve(approx);

    for (qsizetype begin = 1;;)
    {
        const auto end{ptr.indexOf(Slash, begin)};
        const auto atEnd{end == -1};
        const auto raw = atEnd ? ptr.sliced(begin) : ptr.sliced(begin, end - begin);
        if (raw.isEmpty() && !atEnd)
        {
            tokens.clear();
            return std::unexpected(ParseError::EmptyNonTerminalToken);
        }
        const auto decoded = decodeToken(raw);
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
            bool digits = true;
            for (QChar ch : decoded)
            {
                if (ch < u'0' || ch > u'9')
                {
                    digits = false;
                    break;
                }
            }
            if (digits)
                return std::unexpected(ParseError::ArrayIndexOverflow);
            if (decoded.isEmpty() && !raw.isEmpty())
                return std::unexpected(ParseError::InvalidEscapeSequence);
            tokens.push_back(Token{Token::Kind::Key, 0, decoded});
        }
        if (atEnd)
            break;
        begin = end + 1;
    }
    return std::expected<void, ParseError>{};
}

} // namespace json_query::json_pointer::detail
