#include "json-query/json-path/JSONPathParseUtils.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/internal/QtHash.hpp"

#include <QDebug>
#include <limits>
#include <ctre.hpp>

namespace json_query::json_path
{

// ──────────────────────────────────────────────────────────────────────
//  Slice Parsing Implementation
// ──────────────────────────────────────────────────────────────────────

std::optional<Slice> makeSlice(QStringView v)
{
    constexpr qsizetype SENTINEL = std::numeric_limits<qsizetype>::max();

    // integer literal per RFC 9535: optional *minus* sign followed by digits.
    // Plus sign is NOT allowed. Leading zeros forbidden unless the value is exactly 0.
    static constexpr auto integer_literal_pattern = ctre::match<"^(?:0|-[1-9][0-9]*|[1-9][0-9]*)$">;

    qCDebug(jsonPathLog).noquote() << "makeSlice(" << v << ")";

    auto strictParse = [&](QStringView part, std::optional<qsizetype>& out, bool overflowInvalid=false)->bool{
        part = part.trimmed();
        if (part.isEmpty()) {
            out.reset();
            return true;
        }

        // Manual integer-literal validation per RFC 9535 ----------
        if (!integer_literal_pattern(part.toString().toStdString())) {
            qCDebug(jsonPathLog) << "strictParse(" << part << ") invalid integer literal";
            return false;
        }

        // Fast path 64-bit conversion first.
        bool ok = false;
        qlonglong v64 = part.toLongLong(&ok, 10);

        if (!ok) {
            // Conversion overflowed 64-bit
            if (overflowInvalid)
                return false; // illegal for this component

            // Otherwise, clamp to sentinel (treat as ±∞)
            if (!part.isEmpty() && part.front() == u'-')
                out = std::numeric_limits<qsizetype>::min();
            else
                out = std::numeric_limits<qsizetype>::max();
            return true;
        }

        // RFC 9535 §4.2.3: each literal MUST fit in safe-int range.
        constexpr qlonglong SAFE_INT = 9007199254740992LL; // 2^53 per RFC 9535 test expectations
        if (v64 <= -SAFE_INT || v64 >= SAFE_INT)
            return false; // totally out of supported range – selector invalid

        static constexpr qlonglong INT32_MIN_LL = static_cast<qlonglong>(std::numeric_limits<int>::min());
        static constexpr qlonglong INT32_MAX_LL = static_cast<qlonglong>(std::numeric_limits<int>::max());

        if (v64 < INT32_MIN_LL)
            out = std::numeric_limits<qsizetype>::min();
        else if (v64 > INT32_MAX_LL)
            out = std::numeric_limits<qsizetype>::max();
        else
            out = static_cast<qsizetype>(v64);
        return true;
    };

    static constexpr auto sliceFull = ctre::match<"^\\s*(?:(?:0|-[1-9][0-9]*|[1-9][0-9]*))?\\s*(?::\\s*(?:(?:0|-[1-9][0-9]*|[1-9][0-9]*))?\\s*(?:\\s*:\\s*(?:(?:0|-[1-9][0-9]*|[1-9][0-9]*))?\\s*)?)?\\s*$">;
    if (!sliceFull(v.toString().toStdString())) {
        qCDebug(jsonPathLog) << "sliceFull regex failed";
    }
    if (!sliceFull(v.toString().toStdString()))
        return std::nullopt;
    const auto parts = v.split(u':');
    if (parts.size() > 3)
        return std::nullopt; // too many colons
    std::optional<qsizetype> startOpt, endOpt, stepOpt;

    if (parts.size()>0 && !strictParse(parts[0].trimmed(), startOpt, /*overflowInvalid=*/true)) { qCDebug(jsonPathLog) << "strictParse failed for start"; return std::nullopt; }
    if (parts.size()>1 && !strictParse(parts[1].trimmed(), endOpt,   /*overflowInvalid=*/true))   { qCDebug(jsonPathLog) << "strictParse failed for end"; return std::nullopt; }
    if (parts.size()>2 && !strictParse(parts[2].trimmed(), stepOpt, /*overflowInvalid=*/true))  { qCDebug(jsonPathLog) << "strictParse failed for step"; return std::nullopt; }

    qCDebug(jsonPathLog) << "parsed slice startOpt="<<startOpt<<" endOpt="<<endOpt<<" stepOpt="<<stepOpt;

    return Slice{
        .start = startOpt.value_or(0),
        .end   = endOpt.value_or(SENTINEL),
        .step  = stepOpt.value_or(1)
    };
}

// ──────────────────────────────────────────────────────────────────────
//  Key Unescaping Implementation
// ──────────────────────────────────────────────────────────────────────

QString unescapeQuotedKey(QStringView key)
{
    QString result;
    result.reserve(key.size());

    for (qsizetype i = 0; i < key.size(); ++i) {
        QChar c = key[i];
        if (c == u'\\' && i + 1 < key.size()) {
            QChar next = key[i + 1];
            switch (next.unicode()) {
                case u'"':  result.append(u'"'); ++i; break;
                case u'\'': result.append(u'\''); ++i; break;
                case u'\\': result.append(u'\\'); ++i; break;
                case u'/':  result.append(u'/'); ++i; break;
                case u'b':  result.append(u'\b'); ++i; break;
                case u'f':  result.append(u'\f'); ++i; break;
                case u'n':  result.append(u'\n'); ++i; break;
                case u'r':  result.append(u'\r'); ++i; break;
                case u't':  result.append(u'\t'); ++i; break;
                case u'u':
                {
                    // Unicode escape: \uXXXX
                    if (i + 5 < key.size()) {
                        bool ok;
                        QString hexStr = key.mid(i + 2, 4).toString();
                        uint codePoint = hexStr.toUInt(&ok, 16);
                        if (ok) {
                            result.append(QChar(codePoint));
                            i += 5;
                        } else {
                            result.append(c);
                        }
                    } else {
                        result.append(c);
                    }
                    break;
                }
                default:
                    result.append(c);
                    break;
            }
        } else {
            result.append(c);
        }
    }
    return result;
}

// ──────────────────────────────────────────────────────────────────────
//  Key Validation Implementation
// ──────────────────────────────────────────────────────────────────────

bool isValidQuotedKey(QStringView key, QuoteStyle style)
{
    // RFC 9535 allows empty string. We need to validate:
    //  * no unescaped control chars (U+0000–U+001F)
    //  * no unescaped quote char matching the surrounding quotes
    //  * escape sequences limited to JSON set
    //  * \uXXXX sequences must be valid and, if surrogate, appear in valid pairs

    bool expectLowSurrogate = false;

    const QChar quoteChar = (style == QuoteStyle::Single) ? QChar(u'\'') : QChar(u'"');

    for (qsizetype i = 0; i < key.size(); ++i) {
        QChar ch = key[i];

        // Reject literal control codes
        if (ch.unicode() < 0x20)
            return false;

        // Reject unescaped quote matching outer quotes
        if (ch == quoteChar)
            return false;

        if (ch != u'\\') {
            // If we expected a low surrogate but got a non-escape, invalid
            if (expectLowSurrogate)
                return false;
            continue;
        }

        // Escape sequence handling -------------------------------------
        if (i + 1 >= key.size()) return false; // dangling backslash
        QChar esc = key[i + 1];

        // List of permitted single-char escapes per RFC 9535 (JSON set) – but
        // \" is only permitted inside double-quoted keys; \' only inside single-quoted keys.
        const QStringView allowedCommon = QStringView{u"\\/bfnrtu"};
        bool escOk = allowedCommon.contains(esc);
        if (!escOk) {
            if (style == QuoteStyle::Double && esc == u'\"') escOk = true;
            else if (style == QuoteStyle::Single && esc == u'\'') escOk = true;
        }
        if (!escOk)
            return false; // unknown escape

        if (esc == u'u') {
            // Expect exactly four hex digits
            if (i + 5 >= key.size()) return false;
            ushort code = 0;
            for (int k = 1; k <= 4; ++k) {
                QChar h = key[i + 1 + k];
                int val = -1;
                if (h.isDigit()) val = h.digitValue();
                else if (h.toLower() >= u'a' && h.toLower() <= u'f') val = 10 + (h.toLower().unicode() - u'a');
                if (val < 0) return false;
                code = static_cast<ushort>((code << 4) | val);
            }

            // Surrogate handling ------------------------------------
            bool isHighSurrogate = (code >= 0xD800 && code <= 0xDBFF);
            bool isLowSurrogate  = (code >= 0xDC00 && code <= 0xDFFF);

            if (expectLowSurrogate) {
                // We were waiting for a low surrogate
                if (!isLowSurrogate)
                    return false; // not a valid pair
                expectLowSurrogate = false; // pair satisfied
            } else if (isHighSurrogate) {
                // Must be followed by another \uXXXX escape representing a low surrogate
                if (i + 11 >= key.size()) return false; // need another 6 chars: \uXXXX
                if (key[i + 6] != u'\\' || key[i + 7] != u'u') return false; // not another \u
                // We'll check the next loop iteration, set flag
                expectLowSurrogate = true;
            } else if (isLowSurrogate) {
                // Low surrogate without preceding high surrogate
                return false;
            }

            i += 5; // consumed \uXXXX
        } else {
            // Skip the escaped char (\", \\ etc.)
            i += 1; // skip escape target
        }
    }

    // If we ended expecting a low surrogate, invalid
    if (expectLowSurrogate)
        return false;

    return true;
}

// ──────────────────────────────────────────────────────────────────────
//  Index Literal Validation Implementation
// ──────────────────────────────────────────────────────────────────────

bool isValidIndexLiteral(QStringView content)
{
    // RFC 9535 integer literal: optional minus sign followed by digits
    // Plus sign is NOT allowed. Leading zeros forbidden unless value is exactly 0.
    static constexpr auto integer_literal_pattern = ctre::match<"^(?:0|-[1-9][0-9]*|[1-9][0-9]*)$">;
    
    QStringView trimmed = content.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }
    
    return integer_literal_pattern(trimmed.toString().toStdString());
}

// ──────────────────────────────────────────────────────────────────────
//  KeyBuilder Implementation
// ──────────────────────────────────────────────────────────────────────

namespace detail {

std::expected<void, Error> KeyBuilder::push(QString key, bool allowSpace)
{
    if (!allowSpace && key.contains(u' '))
        return std::unexpected(Error::BlankInKey);
    tgt.append(Token{Token::Kind::Key, 0, {}, qt_hash(key), key});
    return {};
}

} // namespace detail

} // namespace json_query::json_path
