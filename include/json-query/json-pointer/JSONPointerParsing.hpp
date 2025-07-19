#pragma once

#include <QString>
#include <QStringView>
#include <QVector>
#include <expected>
#include <limits>
#include <cstring>

namespace json_query::json_pointer::detail {

struct Token {
    enum class Kind : quint8 { Key, Index };
    Kind       kind;
    qsizetype  index{};
    QString    key{};
};

[[nodiscard]] inline QString decodeToken(QStringView token) noexcept
{
    const qsizetype len = token.size();
    const char16_t* src = token.utf16();
    qsizetype firstTilde = 0;
    while (firstTilde < len && src[firstTilde] != u'~')
        ++firstTilde;
    if (firstTilde == len)
        return token.toString();

    QString out(len, Qt::Uninitialized);
    QChar* dst = out.data();
    std::memcpy(dst, src, firstTilde * sizeof(char16_t));
    qsizetype wr = firstTilde;
    qsizetype rd = firstTilde;
    while (rd < len) {
        const char16_t ch = src[rd++];
        if (ch == u'~' && rd < len) {
            switch (src[rd]) {
            case u'0': dst[wr++] = u'~'; ++rd; continue;
            case u'1': dst[wr++] = u'/'; ++rd; continue;
            default: break;
            }
        }
        dst[wr++] = ch;
    }
    out.truncate(wr);
    return out;
}

[[nodiscard]] inline bool parseArrayIndex(const QString& s, qsizetype& out) noexcept
{
    if (s.isEmpty()) return false;
    qsizetype value{0};
    for (const QChar ch : s) {
        if (ch < u'0' || ch > u'9') return false;
        const qsizetype digit = ch.unicode() - u'0';
        if (value > (std::numeric_limits<qsizetype>::max() - digit) / 10) return false;
        value = value * 10 + digit;
    }
    out = value;
    return true;
}

[[nodiscard]] inline bool parsePointer(QStringView ptr, QVector<Token>& tokens) noexcept
{
    tokens.clear();
    constexpr char16_t Slash{u'/'};
    if (ptr.isEmpty()) return true;
    if (ptr.front() != Slash) return false;
    if (ptr.size() == 1) { tokens.append(Token{Token::Kind::Key, 0, QString{}}); return true; }

    const qsizetype approx = ptr.count(Slash);
    tokens.reserve(approx);

    for (qsizetype begin=1;;) {
        const qsizetype end = ptr.indexOf(Slash, begin);
        const bool atEnd = end == -1;
        const QStringView raw = atEnd ? ptr.sliced(begin) : ptr.sliced(begin, end - begin);
        if (raw.isEmpty() && !atEnd) { tokens.clear(); return false; }
        const QString decoded = decodeToken(raw);
        qsizetype idx{};
        if (parseArrayIndex(decoded, idx))
            tokens.append(Token{Token::Kind::Index, idx, {}});
        else
            tokens.append(Token{Token::Kind::Key, 0, decoded});
        if (atEnd) break;
        begin = end + 1;
    }
    return true;
}

} // namespace json_query::json_pointer::detail
