// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-pointer/JSONPointer.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include "json-query/utils/JSONError.hpp"
#include "json-query/json-pointer/JSONPointerParsing.hpp"
#include "json-query/json-pointer/JSONPointerEvaluation.hpp"

#include <charconv> // std::to_chars
#include <cmath>    // std::log10 (for capacity guess)
#include <expected>

namespace json_query::json_pointer
{

// ────────────────────────────────────────────────────────────────────
//  Factory
// ────────────────────────────────────────────────────────────────────
JSONPointer::ParseResult JSONPointer::create(QStringView pointer) noexcept
{
    JSONPointer jp;
    if (auto parseResult{json_pointer::detail::parsePointer(pointer, jp.m_tokens)}; !parseResult)
        return std::unexpected(Error{parseResult.error()});
    return jp;
}

// ────────────────────────────────────────────────────────────
//  Public evaluation with unified error type
// ────────────────────────────────────────────────────────────

namespace
{
using json_pointer::detail::DetailedEvalError;

auto evaluateImpl(const std::vector<json_pointer::Token>& tokens, const QJsonValue& value) noexcept
{
    return json_pointer::detail::evaluatePointerImpl(tokens, value);
}

auto evaluateDocumentImpl(const std::vector<json_pointer::Token>& tokens, const QJsonDocument& doc) noexcept
{
    if (doc.isNull())
        return evaluateImpl(tokens, QJsonValue{});
    if (doc.isObject())
        return evaluateImpl(tokens, QJsonValue{doc.object()});
    if (doc.isArray())
        return evaluateImpl(tokens, QJsonValue{doc.array()});
    return evaluateImpl(tokens, QJsonValue{});
}

JSONPointer::EvalResult toPublicError(const std::expected<QJsonValue, DetailedEvalError>& result)
{
    if (!result)
        return std::unexpected(Error{result.error().error, result.error().tokenIndex});
    return *result;
}
} // namespace

JSONPointer::EvalResult JSONPointer::evaluate(const QJsonDocument& doc) const
{
    return toPublicError(evaluateDocumentImpl(m_tokens, doc));
}

JSONPointer::EvalResult JSONPointer::evaluate(const QJsonValue& value) const
{
    return toPublicError(evaluateImpl(m_tokens, value));
}

QString JSONPointer::to_string() const
{
    if (m_tokens.empty())
        return {};

    // ───────────────────────────────── capacity ─────────────────────────────────
    qsizetype cap{0};
    for (const Token& tk : m_tokens)
    {
        cap += 1; // the leading '/'
        if (tk.kind == Token::Kind::Key)
            cap += tk.key.size() * 2; // worst-case expansion
        else
        { // digits in index
            cap += (tk.index == 0) ? 1
                                   : static_cast<qsizetype>(std::floor(std::log10(static_cast<double>(tk.index))) + 1);
        }
    }

    // ─────────────────────────────── single allocation ──────────────────────────
    QString out(cap, Qt::Uninitialized);
    QChar*  dst{out.data()};
    qsizetype wr{0};

    auto writeIndex = [&](qsizetype value)
    {
        char buf[24];
        auto [ptr, ec]{std::to_chars(std::begin(buf), std::end(buf), value)};
        const auto len{ptr - buf};
        for (qsizetype i = 0; i < len; ++i)
            dst[wr++] = QLatin1Char(buf[i]);
    };

    // ─────────────────────────────── encode segments ────────────────────────────
    for (const Token& tk : m_tokens)
    {
        dst[wr++] = u'/';

        if (tk.kind == Token::Kind::Key)
        {
            const QChar* src{tk.key.constData()};
            const auto   n{tk.key.size()};

            for (qsizetype i = 0; i < n; ++i)
            {
                switch (src[i].unicode())
                {
                case u'~':
                    dst[wr++] = u'~';
                    dst[wr++] = u'0';
                    break;
                case u'/':
                    dst[wr++] = u'~';
                    dst[wr++] = u'1';
                    break;
                default:
                    dst[wr++] = src[i];
                    break;
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

} // namespace json_query::json_pointer
