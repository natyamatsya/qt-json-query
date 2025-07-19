#include "json-query/JSONPointer.hpp"
#include "json-query/JSONQueryUtils.hpp"
#include "json-query/json-pointer/JSONPointerParsing.hpp"
#include "json-query/json-pointer/JSONPointerEvaluation.hpp"
#include <charconv>   // std::to_chars
#include <cmath>      // std::log10 (for capacity guess)
#include <expected>

namespace json_query {

// ────────────────────────────────────────────────────────────────────
//  Factory
// ────────────────────────────────────────────────────────────────────
JSONPointer::Result JSONPointer::create(QStringView pointer)
{
    JSONPointer jp;
    if (auto res = json_pointer::detail::parsePointer(pointer, jp.m_tokens); !res)
        return std::unexpected(res.error());
    return jp;
}

// ────────────────────────────────────────────────────────────
//  Public evaluation with detailed error
// ────────────────────────────────────────────────────────────

JSONPointer::EvalResult JSONPointer::evaluate(QJsonDocument const& doc) const
{
    if (doc.isNull())
        return evaluate(QJsonValue{});
    if (doc.isObject())
        return evaluate(QJsonValue{ doc.object() });
    if (doc.isArray())
        return evaluate(QJsonValue{ doc.array() });
    // otherwise, treat as undefined
    return evaluate(QJsonValue{});
}

JSONPointer::EvalResult JSONPointer::evaluate(QJsonValue const& value) const
{
    auto res = json_pointer::detail::evaluatePointer(m_tokens, value);
    if (res)
        return res.value();
    return std::unexpected(res.error());
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

} // namespace json_query
