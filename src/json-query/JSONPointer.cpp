#include "json-query/JSONPointer.hpp"
#include "json-query/JSONQueryUtils.hpp"

#include <charconv>   // std::to_chars
#include <cmath>      // std::log10 (for capacity guess)

namespace json_query {

JSONPointer::JSONPointer(const QString &pointer)
{
    parsePointer(pointer);
}

QJsonValue JSONPointer::evaluate(const QJsonDocument &document) const
{
    // Prefer array if explicitly an array; otherwise treat as object (default)
    if (!document.isArray())
        return evaluate(document.object());

    return evaluate(document.array());
}

QJsonValue JSONPointer::evaluate(const QJsonValue &value) const
{
    if (!isValid())
        return {QJsonValue::Undefined};

    // Empty path returns the value itself
    if (m_tokens.isEmpty())
        return value;

    return evaluateInternal(value);
}

namespace
{
    // anonymous: internal linkage

    [[nodiscard]] inline QString
    decodeToken(QStringView const token) noexcept
    {
        // Fast path: nothing to unescape
        const qsizetype len = token.size();
        const char16_t* src = token.utf16();

        qsizetype firstTilde = 0;
        while (firstTilde < len && src[firstTilde] != u'~')
            ++firstTilde;

        if (firstTilde == len)
            return token.toString();                // no escapes at all

        // -----------------------------------------------------------------
        // Allocate *once* with worst-case size, copy the prefix in bulk
        // -----------------------------------------------------------------
        QString out(len, Qt::Uninitialized);
        QChar*  dst = out.data();

        std::memcpy(dst, src, firstTilde * sizeof(char16_t));
        qsizetype wr = firstTilde;                  // write cursor
        qsizetype rd = firstTilde;                  // read  cursor

        // -----------------------------------------------------------------
        // Decode the remainder
        // -----------------------------------------------------------------
        while (rd < len)
        {
            const char16_t ch = src[rd++];

            if (ch == u'~' && rd < len) [[likely]]
            {
                switch (src[rd])
                {
                case u'0':  dst[wr++] = u'~'; ++rd; continue;
                case u'1':  dst[wr++] = u'/'; ++rd; continue;
                default:    break;              // fall through → copy '~'
                }
            }
            dst[wr++] = ch;                         // literal copy
        }

        out.truncate(wr);                           // shrink to real size
        return out;
    }

    // ─────────────────────────────────────────────────────────────────────
    //  Locale-free, overflow-safe decimal parser           (unchanged)
    // ─────────────────────────────────────────────────────────────────────
    [[nodiscard]] inline bool parseArrayIndex(QString const& s, qsizetype& out) noexcept
    {
        if (s.isEmpty()) return false;

        qsizetype value{0};
        for (const QChar ch : s)
        {
            if (ch < u'0' || ch > u'9')
                return false;

            const qsizetype digit = ch.unicode() - u'0';
            if (value > (std::numeric_limits<qsizetype>::max() - digit) / 10)
                return false;                    // overflow
            value = value * 10 + digit;
        }
        out = value;
        return true;
    }
}

void JSONPointer::parsePointer(QStringView const ptr)
{
    m_tokens.clear();
    m_valid = true;

    [[maybe_unused]] constexpr char16_t Slash{ u'/' };

    // Empty string  →  whole document
    if (ptr.isEmpty()) [[unlikely]]
        return;

    // Must begin with '/'
    if (ptr.front() != Slash) [[unlikely]] {
        m_valid = false;
        return;
    }

    // Single '/' → token is empty string
    if (ptr.size() == 1) {
        m_tokens.append(Token{ Token::Kind::Key, 0, QString{} });
        return;
    }

    // Upper bound: every '/' may introduce a token
    const qsizetype approxTokens{ ptr.count(Slash) };
    m_tokens.reserve(approxTokens);

    for (qsizetype begin{ 1 }; /*loop*/; )
    {
        const qsizetype end   { ptr.indexOf(Slash, begin) };
        const bool      atEnd { end == -1 };

        const QStringView rawSeg = atEnd
                                 ? ptr.sliced(begin)
                                 : ptr.sliced(begin, end - begin);

        // Empty segment *inside* the pointer is invalid (“//”)
        if (rawSeg.isEmpty() && !atEnd) [[unlikely]] {
            m_valid = false;
            m_tokens.clear();
            return;
        }

        // RFC-6901 unescaping
        const QString decoded = decodeToken(rawSeg);

        // Decide Key vs Index once, store appropriately
        qsizetype idxVal{};
        if (parseArrayIndex(decoded, idxVal)) {
            m_tokens.append(Token{ Token::Kind::Index, idxVal, {} });
        } else {
            m_tokens.append(Token{ Token::Kind::Key,   0,     decoded });
        }

        if (atEnd)
            break;                  // finished last segment

        begin = end + 1;            // jump past the slash
    }
}

namespace
{
    // ────────────────────────────────────────────────────────────────────
    //  Inline helpers
    // ────────────────────────────────────────────────────────────────────
    [[nodiscard]] inline bool stepObject(QJsonValue& current, QString const& key) noexcept
    {
        const QJsonObject obj{ current.toObject() };
        const auto        it { obj.constFind(key) };
        if (it == obj.constEnd())  return false;

        current = *it;
        return true;
    }

    [[nodiscard]] inline bool stepArray(QJsonValue& current, qsizetype index) noexcept
    {
        const QJsonArray arr{ current.toArray() };
        if (index < 0 || index >= arr.size())  return false;

        current = arr.at(index);
        return true;
    }
} // anonymous namespace

// ────────────────────────────────────────────────────────────────────
//  JSONPointer::evaluateInternal()
//  Iterative, no conversions, branch-light
// ────────────────────────────────────────────────────────────────────
QJsonValue JSONPointer::evaluateInternal(QJsonValue const& root) const
{
    QJsonValue current{ root };

    for (Token const& tk : m_tokens) // cache-friendly AoS loop
    {
        switch (current.type())
        {
            // ─────────── Objects ───────────
            case QJsonValue::Object:
                if (tk.kind != Token::Kind::Key) [[unlikely]]
                    return QJsonValue{ QJsonValue::Undefined };

                if (!stepObject(current, tk.key)) [[unlikely]]
                    return QJsonValue{ QJsonValue::Undefined };
                break;

            // ─────────── Arrays ────────────
            case QJsonValue::Array:
                if (tk.kind != Token::Kind::Index) [[unlikely]]
                    return QJsonValue{ QJsonValue::Undefined };

                if (!stepArray(current, tk.index)) [[unlikely]]
                    return QJsonValue{ QJsonValue::Undefined };
                break;

            // ─────────── Scalars / Null ────
        default:
            return QJsonValue{ QJsonValue::Undefined };
        }
    }
    return current;                            // all tokens consumed
}

QString JSONPointer::toString() const
{
    if (!m_valid || m_tokens.isEmpty())
        return {};

    // ───────────────────────────────── capacity ─────────────────────────────────
    qsizetype cap = 0;
    for (const Token& tk : m_tokens) {
        cap += 1;                         // the leading '/'
        if (tk.kind == Token::Kind::Key)
            cap += tk.key.size() * 2;     // worst-case expansion
        else {                            // digits in index
            cap += (tk.index == 0) ? 1
                                   : static_cast<qsizetype>(
                                         std::floor(std::log10(
                                             static_cast<double>(tk.index))) + 1);
        }
    }

    // ─────────────────────────────── single allocation ──────────────────────────
    QString out(cap, Qt::Uninitialized);
    QChar*  dst = out.data();
    qsizetype wr = 0;

    auto writeIndex = [&](qsizetype value) {
        char buf[24];
        auto [ptr, ec] = std::to_chars(std::begin(buf), std::end(buf), value);
        const qsizetype len = ptr - buf;
        for (qsizetype i = 0; i < len; ++i)
            dst[wr++] = QLatin1Char(buf[i]);
    };

    // ─────────────────────────────── encode segments ────────────────────────────
    for (const Token& tk : m_tokens)
    {
        dst[wr++] = u'/';

        if (tk.kind == Token::Kind::Key)
        {
            const QChar* src = tk.key.constData();
            const qsizetype n = tk.key.size();

            for (qsizetype i = 0; i < n; ++i) {
                switch (src[i].unicode()) {
                case u'~': dst[wr++] = u'~'; dst[wr++] = u'0'; break;
                case u'/': dst[wr++] = u'~'; dst[wr++] = u'1'; break;
                default  : dst[wr++] = src[i];                 break;
                }
            }
        }
        else
        {
            writeIndex(tk.index);
        }
    }

    out.truncate(wr);
    return out;
}

} // namespace json_query
