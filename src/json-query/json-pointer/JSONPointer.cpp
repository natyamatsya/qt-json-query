#include "json-query/JSONPointer.hpp"
#include "json-query/JSONQueryUtils.hpp"
#include "json-query/json-pointer/JSONPointerParsing.hpp"
#include "json-query/json-pointer/JSONPointerEvaluation.hpp"

#include <charconv>   // std::to_chars
#include <cmath>      // std::log10 (for capacity guess)

namespace json_query {

// ────────────────────────────────────────────────────────────────────
//  Factory
// ────────────────────────────────────────────────────────────────────
std::expected<JSONPointer, JSONPointer::Error> JSONPointer::create(QStringView pointer)
{
    JSONPointer jp;
    auto res = jp.parsePointer(pointer);
    if (!res)
        return std::unexpected(res.error());
    return jp;
}

std::expected<void, JSONPointer::Error> JSONPointer::parsePointer(QStringView ptr)
{
    auto res = json_pointer::detail::parsePointer(ptr, m_tokens);
    if (!res)
        return std::unexpected(mapError(res.error()));
    return {};
}

QJsonValue JSONPointer::evaluate(const QJsonDocument &document) const
{
    return document.isArray()
             ? evaluate(document.array())
             : evaluate(document.object());
}

QJsonValue JSONPointer::evaluate(const QJsonValue &value) const
{
    return json_pointer::detail::evaluatePointer(m_tokens, value);
}

// evaluateInternal no longer needed; but keep thin wrapper for legacy internal call
QJsonValue JSONPointer::evaluateInternal(QJsonValue const& root) const
{
    return json_pointer::detail::evaluatePointer(m_tokens, root);
}

QString JSONPointer::toString() const
{
    if (m_tokens.isEmpty())
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

JSONPointer::Error JSONPointer::mapError(json_pointer::detail::ParseError pe)
{
    using PE = json_pointer::detail::ParseError;
    switch (pe) {
    case PE::MissingLeadingSlash:    return Error::MissingLeadingSlash;
    case PE::EmptyNonTerminalToken:  return Error::EmptyNonTerminalToken;
    case PE::InvalidEscapeSequence:  return Error::InvalidEscapeSequence;
    case PE::ArrayIndexOverflow:     return Error::ArrayIndexOverflow;
    default:                         return Error::InvalidEscapeSequence; // fallback
    }
}

} // namespace json_query
