#include "json-query/json-path/JSONPathCompile.hpp"

#include <iostream>
#include <QDebug>
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"

#include "json-query/json-path/internal/QtHash.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include "json-query/json-path/JSONPathFilter.hpp"  // For compileFilter implementation

#include <limits>
#include <QRegularExpression>

namespace json_query::json_path
{

// ──────────────────────────────────────────────────────────────────────
//  Helpers local to this compilation unit
// ──────────────────────────────────────────────────────────────────────
namespace
{
    // ─── Quoted-key validator (RFC 9535) ───────────────────────────────
    enum class QuoteStyle { Single, Double };

    // Parse start:end:step  (parts may be empty) --------------------------------
    [[nodiscard]] static inline std::optional<Slice> makeSlice(QStringView v)
    {
        constexpr qsizetype SENTINEL = std::numeric_limits<qsizetype>::max();

        // integer literal per RFC 9535: optional *minus* sign followed by digits.
        // Plus sign is NOT allowed. Leading zeros forbidden unless the value is exactly 0.
        static const QRegularExpression re(R"(^(?:0|-[1-9][0-9]*|[1-9][0-9]*)$)");

        qCDebug(jsonPathLog).noquote() << "makeSlice(" << v << ")";

        auto strictParse = [&](QStringView part, std::optional<qsizetype>& out, bool overflowInvalid=false)->bool{
            part = part.trimmed();
            if (part.isEmpty()) {
                out.reset();
                return true; // omitted component
            }

            // Manual integer-literal validation per RFC 9535 ----------
            qsizetype idx = 0;
            if (part[idx] == u'-') ++idx; // optional minus
            if (idx >= part.size() || !part[idx].isDigit()) return false;
            if (part[idx] == u'0' && (part.size() - idx) > 1) return false; // leading zero forbidden
            for (qsizetype j = idx; j < part.size(); ++j) {
                if (!part[j].isDigit()) return false;
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

        static const QRegularExpression sliceFull(
            R"(^\s*(?:(?:0|-[1-9][0-9]*|[1-9][0-9]*))?\s*(?::\s*(?:(?:0|-[1-9][0-9]*|[1-9][0-9]*))?\s*(?::\s*(?:(?:0|-[1-9][0-9]*|[1-9][0-9]*))?\s*)?)?\s*$)");
        if (!sliceFull.matchView(v).hasMatch()) {
            qCDebug(jsonPathLog) << "sliceFull regex failed";
        }
        if (!sliceFull.matchView(v).hasMatch())
            return std::nullopt;
        const auto parts = v.split(u':');
        if (parts.size() > 3)
            return std::nullopt; // too many colons
        std::optional<qsizetype> startOpt, endOpt, stepOpt;

        if (parts.size()>0 && !strictParse(parts[0].trimmed(), startOpt, /*overflowInvalid=*/true)) { qCDebug(jsonPathLog) << "strictParse failed for start"; return std::nullopt; }
        if (parts.size()>1 && !strictParse(parts[1].trimmed(), endOpt,   /*overflowInvalid=*/true))   { qCDebug(jsonPathLog) << "strictParse failed for end"; return std::nullopt; }
        if (parts.size()>2 && !strictParse(parts[2].trimmed(), stepOpt, /*overflowInvalid=*/true))  { qCDebug(jsonPathLog) << "strictParse failed for step"; return std::nullopt; }

        qCDebug(jsonPathLog) << "parsed slice startOpt="<<startOpt<<" endOpt="<<endOpt<<" stepOpt="<<stepOpt;

        // Zero-step slices are *valid* but yield an empty result (§4.2.3).
        // We pass them through to the evaluator unchanged.

        qsizetype step = stepOpt.value_or(1);

        // Out-of-range literals are clamped to ±∞ above; evaluation will handle them.

        qsizetype start = startOpt.has_value() ? *startOpt : SENTINEL;
        qsizetype end   = endOpt.has_value()   ? *endOpt   : SENTINEL;

        return Slice{start,end,step};
    }

    // --- Helper: unescape quoted key according to RFC 9535 ----------------
    static QString unescapeQuotedKey(QStringView key)
    {
        QString out;
        out.reserve(key.size());
        for (qsizetype i = 0; i < key.size(); ++i) {
            QChar c = key[i];
            if (c != u'\\') {
                out.append(c);
                continue;
            }
            if (i + 1 >= key.size()) { // dangling backslash, keep literally
                out.append(QStringLiteral("\\"));
                break;
            }
            QChar n = key[++i];
            switch (n.unicode()) {
            case u'\\': out.append(u'\\'); break;
            case u'"':  out.append(u'"');  break;
            case u'\'': out.append(u'\''); break;
            case u'/':  out.append(u'/');   break;
            case u'b':  out.append(u'\b'); break;
            case u'f':  out.append(u'\f'); break;
            case u'n':  out.append(u'\n'); break;
            case u'r':  out.append(u'\r'); break;
            case u't':  out.append(u'\t'); break;
            case u'u': {
                if (i + 4 >= key.size()) {
                    out.append(QStringLiteral("\\u"));
                    break;
                }
                bool ok = false;
                ushort code = QString(key.mid(i + 1, 4)).toUShort(&ok, 16);
                if (!ok) {
                    out.append(QStringLiteral("\\u"));
                    break;
                }
                out.append(QChar(code));
                i += 4;
                break;
            }
            default:
                // Unknown escape, keep literally
                out.append(n);
            }
        }
        return out;
    }

    // Adjusted validation to allow escapes and empty key
    [[nodiscard]] static bool isValidQuotedKey(QStringView key, QuoteStyle style)
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

    // Helper to validate integer literals per RFC 9535 §4.2.3
    [[nodiscard]] static bool isValidIndexLiteral(QStringView content)
    {
        static const QRegularExpression re(R"(^(?:0|-[1-9][0-9]*|[1-9][0-9]*)$)");
        auto m = re.match(content.toString());
        if (!m.hasMatch()) {
            qCDebug(jsonPathLog) << "isValidIndexLiteral(" << content << ") match=false";
            return false;
        }
        bool ok=false; qlonglong val = content.toLongLong(&ok);
        qCDebug(jsonPathLog) << "isValidIndexLiteral(" << content << ") match=true ok=" << ok << " val=" << (ok?QString::number(val):QStringLiteral("n/a"));
        constexpr qlonglong SAFE_INT = 9007199254740992LL; // 2^53 per RFC 9535 test expectations
        if (!ok || val <= -SAFE_INT || val >= SAFE_INT)
            return false; // totally out of supported range – selector invalid

        return true;
    }

} // namespace

namespace detail {

// ────────────────────────────────────────────────────────────────
// 1. pushKey helper (renamed)
// ────────────────────────────────────────────────────────────────
struct KeyBuilder {
    QVector<Token>& tgt;

    std::expected<void,Error> push(QString key, bool allowSpace = false)
    {
        if (!allowSpace && key.contains(u' '))
            return std::unexpected(Error::BlankInKey);
        tgt.append(Token{Token::Kind::Key, 0, {}, qt_hash(key), key});
        return {};
    }
};

// ────────────────────────────────────────────────────────────────
// 2. dot‑segment parser (no switch on QChar)
// ────────────────────────────────────────────────────────────────
[[nodiscard]] std::expected<qsizetype,Error>
parseDot(qsizetype pos, QStringView sv,
         KeyBuilder& kb, QVector<Token>& tokens)
{
    qCDebug(jsonPathLog) << "parseDot pos=" << pos;
    const qsizetype n = sv.size();
    if (++pos >= n) return std::unexpected(Error::TrailingDot);

    QChar nxt = sv[pos];
    qCDebug(jsonPathLog) << "parseDot: processing character '" << nxt << "' at pos=" << pos;
    
    if (nxt == u'.') {
        qCDebug(jsonPathLog) << "parseDot: found recursive segment (..)";
        tokens.append(Token{Token::Kind::Recursive});
        ++pos;
        if (pos >= n) return std::unexpected(Error::TrailingRecursive);
        return pos;
    }
    if (nxt == u'*') { 
        qCDebug(jsonPathLog) << "parseDot: found wildcard (*)";
        tokens.append(Token{Token::Kind::Wildcard }); 
        ++pos; 
        return pos; 
    }
    qsizetype start = pos;
    while (pos < n && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
    
    // RFC 9535 validation: member-name-shorthand must follow ABNF grammar
    // name-first = ALPHA / "_" / Unicode, not DIGIT
    // name-char = name-first / DIGIT
    QStringView identifier = sv.sliced(start, pos - start);
    
    // Check first character: must be ALPHA, underscore, or Unicode (not digit)
    QChar first = identifier[0];
    if (first.isDigit()) {
        qCDebug(jsonPathLog) << "parseDot: rejecting numeric identifier" << identifier;
        return std::unexpected(Error::InvalidIdentifier);
    }
    if (!first.isLetter() && first != u'_' && first.unicode() < 0x80) {
        qCDebug(jsonPathLog) << "parseDot: rejecting invalid identifier" << identifier;
        return std::unexpected(Error::InvalidIdentifier);
    }
    
    // Check remaining characters: must be name-first or DIGIT
    for (qsizetype i = 1; i < identifier.size(); ++i) {
        QChar ch = identifier[i];
        if (!ch.isLetterOrNumber() && ch != u'_') {
            qCDebug(jsonPathLog) << "parseDot: rejecting identifier with invalid character" << identifier;
            return std::unexpected(Error::InvalidIdentifier);
        }
    }
    
    if (auto r = kb.push(identifier.toString()); !r)
        return std::unexpected(r.error());
    return pos;
}

// A small façade so every rule can emit tokens consistently
// BracketSink now carries *filters* as well
struct BracketSink {
    QVector<Token>&   tk;
    KeyBuilder&       kb;
    QVector<ContextFilterFn>& contextFilters;
    QVector<FilterFn>& filters;
    int               currentBracketGroupId; // Track current bracket group ID

    std::expected<void,Error> key(QString key, bool allow=false) { 
        // Create token with bracket group ID
        if (!allow && key.contains(u' '))
            return std::unexpected(Error::BlankInKey);
        Token t{Token::Kind::Key, 0, {}, qt_hash(key), key};
        t.bracketGroupId = currentBracketGroupId;
        tk.append(std::move(t));
        return {};
    }
    void keyList(const QVector<QString>& keys)
    {
        if (keys.isEmpty()) return;

        Token t;
        t.kind = Token::Kind::KeyList;
        t.bracketGroupId = currentBracketGroupId; // Set bracket group ID

        // Pack the keys into a single QString separated by '\n' so that the
        // evaluator can split them later without ambiguity.
        QStringList list;
        list.reserve(keys.size());
        for (const QString& k : keys)
            list.append(k);

        t.key = list.join(u"\n");
        tk.append(std::move(t));
    }
    void wild()                 { 
        Token t{Token::Kind::Wildcard};
        t.bracketGroupId = currentBracketGroupId;
        tk.append(t); 
    }
    void slice(const Slice& s)   { 
        Token t{Token::Kind::Slice,0,s,0u};
        t.bracketGroupId = currentBracketGroupId;
        tk.append(t); 
    }
    void index(int i)            { 
        qCDebug(jsonPathLog) << "BracketSink::index emit" << i;
        Token t{Token::Kind::Index,i};
        t.bracketGroupId = currentBracketGroupId;
        tk.append(t); 
    }
    void pushFilter(const Token& t){ 
        Token copy = t;
        copy.bracketGroupId = currentBracketGroupId;
        tk.append(copy); 
    }
};

// A helper to iterate over the content and run rules.
using BrRule = std::function<std::optional<Error>(QStringView, BracketSink&)>;

static const std::array<BrRule,10> BR_RULES = {{
    // 0.  union by comma (sequential selectors, e.g. ?@.a,1) --------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.contains(u',')) return std::nullopt;

        // Parse comma-separated selectors
        using json_query::json_path::detail::splitTopLevel;
        auto parts = splitTopLevel(content.toString(), QLatin1StringView(","));
        if (!parts) return std::nullopt;

        // Handle multiple parts
        QVector<QStringView> segs;
        const auto& [lhs, rhs] = *parts;
        segs.push_back(lhs.trimmed());
        segs.push_back(rhs.trimmed());
        
        // TODO: splitTopLevel currently only handles two parts, but RFC 9535 allows more
        // For now, we handle the common two-part case correctly

        // Helper lambda to route a single segment through existing rules (skip union rule to prevent recursion)
        auto compileOne = [&](QStringView seg)->std::optional<Error> {
            // Case A: segment starts with '?'  → standalone filter expression
            if (seg.startsWith(u'?')) {
                QString expr = QString(seg.mid(1)).trimmed();
                if (expr.isEmpty()) return std::optional<Error>{};
                if (auto tok = json_query::json_path::compileContextFilter(expr, out.contextFilters, out.filters)) {
                    out.pushFilter(*tok);
                    return Error::Ok;
                }
                return std::optional<Error>{}; // treat as non-match so other rules can try
            }

            // Case B: delegate to other bracket rules (skip union rule itself)
            for (size_t i = 1; i < std::size(BR_RULES); ++i) {
                if (auto err = BR_RULES[i](seg, out)) {
                    return err; // either Ok or specific error
                }
            }
            return std::nullopt; // no rule matched
        };

        for (auto sv : segs) {
            if (auto maybe = compileOne(sv); !maybe) return std::nullopt;
            else if (*maybe != Error::Ok) return maybe; // propagate
        }
        return Error::Ok;
    },

    // 1.  *      ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (content != u"*") return std::nullopt;
        out.wild();
        return Error::Ok;
    },

    // 2.  123    ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        qCDebug(jsonPathLog) << "BR_RULE index-single check" << content.toString();
        if (!isValidIndexLiteral(content)) {
            qCDebug(jsonPathLog) << "  -> invalid index literal";
            return std::nullopt;
        }
        qCDebug(jsonPathLog) << "  -> valid index literal";
        bool ok=false; qlonglong val = content.toLongLong(&ok);
        int idx = (val > std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
                 : (val < std::numeric_limits<int>::min()) ? std::numeric_limits<int>::min()
                 : static_cast<int>(val);
        qCDebug(jsonPathLog) << "  emitting index token" << idx;
        out.index(idx);
        return Error::Ok;
    },

    // 2b.  1,2,3  --------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        qCDebug(jsonPathLog) << "BR_RULE index-list raw" << content.toString();
        if (!content.contains(u',')) return std::nullopt;
        // Quick check: no quotes or slice colons
        if (content.contains(u'\'') || content.contains(u'"') || content.contains(u':'))
            return std::nullopt;
        const auto parts = content.split(u',');
        for (QStringView p : parts)
        {
            QStringView t = p.trimmed();
            if (!isValidIndexLiteral(t)) {
                qCDebug(jsonPathLog) << "  list element invalid" << t.toString();
                return std::nullopt; // not pure index list
            }
            bool ok=false; qlonglong val = t.toLongLong(&ok);
            int idx = (val > std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
                     : (val < std::numeric_limits<int>::min()) ? std::numeric_limits<int>::min()
                     : static_cast<int>(val);
            qCDebug(jsonPathLog) << "  list element emit" << idx;
            out.index(idx);
        }
        return Error::Ok;
    },

    // 3.  1:3:2  ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.contains(u':')) return std::nullopt;
        {
            if (auto maybe = makeSlice(content)) {
                out.slice(*maybe);
                return Error::Ok;
            }
            // Return nullopt instead of Error::InvalidSlice to allow other rules to try
            // This fixes slice existence tests like $[?@[0:2]] which should be handled by filter rules
            return std::nullopt;
        }
    },

    // 4.  ?(...) ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.startsWith(u"?(") || !content.endsWith(u')'))
            return std::nullopt;

        std::cout << "[bracket ?(] raw content=" << content.toString().toStdString() << std::endl;

        QString expr = content.sliced(2, content.size() - 3).toString();
        std::cout << "[bracket ?(] extracted expr=" << expr.toStdString() << std::endl;

        if (auto tok = json_query::json_path::compileContextFilter(expr, out.contextFilters, out.filters)) {
            out.pushFilter(*tok);
            return Error::Ok;
        }
        return Error::UnsupportedFilter;
    },

    // 4b.  ?expr (no outer parentheses) --------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        // Accept syntax like ?@.a==1  or ?$==null (no wrapping parens)
        if (!content.startsWith(u'?'))
            return std::nullopt; // Not a filter expression

        QStringView exprView = content.sliced(1).trimmed();
        if (exprView.isEmpty()) return std::nullopt; // Could be placeholder handled later

        QString expr = exprView.toString();
        qCDebug(jsonPathLog) << "BR_RULE 6: calling compileContextFilter with expr:" << expr;
        if (auto tok = json_query::json_path::compileContextFilter(expr, out.contextFilters, out.filters)) {
            qCDebug(jsonPathLog) << "BR_RULE 6: compileContextFilter succeeded";
            out.pushFilter(*tok);
            return Error::Ok;
        }
        qCDebug(jsonPathLog) << "BR_RULE 6: compileContextFilter failed, returning UnsupportedFilter";
        return Error::UnsupportedFilter;
    },

    // 5.  placeholder '?' or list '?,?' ------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.contains(u'?')) return std::nullopt;
        // verify pattern consists only of '?' separated by commas/spaces
        for (QStringView part : content.split(u',')) {
            if (part.trimmed() != u"?") return std::nullopt; // not pure placeholder list
        }
        // For each placeholder, create a no-op filter token that will be
        // resolved later (currently always-true).
        for ([[maybe_unused]] QStringView _ : content.split(u',')) {
            auto alwaysTrue = [](const QJsonValue& currentNode, const QJsonValue& /*root*/) { return true; };
            out.contextFilters.append(alwaysTrue);
            Token t{Token::Kind::Filter};
            t.contextFilterId = out.contextFilters.size() - 1;
            out.pushFilter(t);
        }
        return Error::Ok;
    },

    // 6.  'key'  ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (content.size() < 2) return std::nullopt;
        QChar q = content.front();
        if ((q != u'\'' && q != u'"') || content.back() != q)
            return std::nullopt;

        QStringView unquoted = content.sliced(1, content.size() - 2);
        QuoteStyle st = (q==u'\'') ? QuoteStyle::Single : QuoteStyle::Double;
        if (!isValidQuotedKey(unquoted, st))
            return Error::InvalidSlice; // reuse generic
        QString unesc = unescapeQuotedKey(unquoted);
        if (auto r = out.key(unesc, true); !r)
            return r.error();
        return Error::Ok;
    },

    // 7.  key (unquoted) — RFC 9535 forbids unquoted string literals inside []
    [](QStringView /*content*/, BracketSink& /*out*/)->std::optional<Error> {
        return std::nullopt; // disallowed per spec
    }
}};

// ────────────────────────────────────────────────────────────────
// 3. bracket parser
// ────────────────────────────────────────────────────────────────
[[nodiscard]] std::expected<qsizetype,Error>
parseBracket(qsizetype pos, QStringView sv,
             KeyBuilder& kb, QVector<Token>& tokens,
             QVector<ContextFilterFn>& contextFilters,
             QVector<FilterFn>& filters)
{
    qCDebug(jsonPathLog) << "parseBracket pos=" << pos;
    const qsizetype n = sv.size();
    if (++pos >= n) return std::unexpected(Error::UnmatchedBracket);

    qsizetype start = pos;
    int bracketLevel = 0;
    while (pos < n) {
        if (sv[pos] == u'[') {
            ++bracketLevel;
            qCDebug(jsonPathLog) << "scan '[' level=" << bracketLevel << " at pos=" << pos;
        } else if (sv[pos] == u']') {
            if (bracketLevel == 0) break; // Found the matching closing bracket
            qCDebug(jsonPathLog) << "scan ']' level=" << bracketLevel << " at pos=" << pos;
            --bracketLevel;
        } else if (sv[pos] == u'\'' || sv[pos] == u'"') {
            const QChar quote = sv[pos++];
            qCDebug(jsonPathLog) << "enter quote" << quote << " at pos=" << pos - 1;

            // Search for the next *unescaped* matching quote.
            while (pos < n) {
                // Find next occurrence of the quote
                qsizetype next = sv.indexOf(quote, pos);
                if (next == -1) {
                    return std::unexpected(Error::UnmatchedQuote);
                }

                // Count preceding backslashes to determine if the quote is escaped.
                int backslashCount = 0;
                for (qsizetype k = next - 1; k >= 0 && sv[k] == u'\\'; --k) {
                    ++backslashCount;
                }
                if (backslashCount % 2 == 0) {
                    // even number of backslashes → quote is *not* escaped
                    pos = next; // leave pos at the quote; the outer ++pos will move past it
                    break;
                }
                // Quote was escaped – continue searching after it
                pos = next + 1;
            }
            qCDebug(jsonPathLog) << "exit quote at pos=" << pos;
        }
        ++pos;
    }
    if (pos >= n) return std::unexpected(Error::UnmatchedBracket);
    qCDebug(jsonPathLog) << "matched bracket end at pos=" << pos;

    QStringView raw = sv.sliced(start, pos - start);
    // trim leading/trailing whitespace
    qsizetype l=0, r=raw.size();
    while (l<r && raw[l].isSpace()) ++l;
    while (r>l && raw[r-1].isSpace()) --r;
    QStringView content = raw.sliced(l, r - l);

    qCDebug(jsonPathLog) << "parseBracket content=" << content.toString();

    BracketSink sink{tokens, kb, contextFilters, filters};
    sink.currentBracketGroupId = tokens.size(); // Set unique bracket group ID

    int ruleIndex = 0;
    for (auto& rule : BR_RULES) {
        qCDebug(jsonPathLog) << "trying BR_RULE" << ruleIndex;
        if (auto err = rule(content, sink)) {
            qCDebug(jsonPathLog) << "BR_RULE" << ruleIndex << "returned error" << static_cast<int>(*err);
            if (*err != Error::Ok) return std::unexpected(*err);
            qCDebug(jsonPathLog) << "BR_RULE" << ruleIndex << "succeeded, returning pos" << (pos + 1);
            return pos + 1;
        }
        qCDebug(jsonPathLog) << "BR_RULE" << ruleIndex << "returned nullopt (no match)";
        ++ruleIndex;
    }
    qCDebug(jsonPathLog) << "all BR_RULES failed, returning UnsupportedFilter";
    return std::unexpected(Error::UnsupportedFilter);
}

// bare‑name parser: replace kb.pushKey → kb.push
std::expected<qsizetype,Error> parseBare(qsizetype pos, QStringView sv, KeyBuilder& kb)
{
    qsizetype start = pos;
    while (pos < sv.size() && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
    if (pos == start) return std::unexpected(Error::EmptySegment);
    
    QStringView identifier = sv.sliced(start, pos - start);
    
    // Special case: if the identifier is exactly '*', create a Wildcard token
    if (identifier == u"*") {
        kb.tgt.append(Token{Token::Kind::Wildcard});
        return pos;
    }
    
    // Otherwise, create a regular Key token
    if (auto r = kb.push(identifier.toString()); !r)
        return std::unexpected(r.error());
    return pos;
}

} // namespace detail

// ──────────────────────────────────────────────────────────────────────
//  detectTrailingFunction - moved from JSONPath
// ──────────────────────────────────────────────────────────────────────
FunctionType detectTrailingFunction(QString& path)
{
    using enum FunctionType;

    static const QPair<QString, FunctionType> table[] = {
        {".length()", Length},
        {".min()", Min},
        {".max()", Max},
    };

    for (auto& p : table)
        if (path.endsWith(p.first))
        {
            path.chop(p.first.size());
            return p.second;
        }

    return None;
}

// ──────────────────────────────────────────────────────────────────────
//  compilePath - moved from JSONPath
// ──────────────────────────────────────────────────────────────────────
std::expected<Compiled, Error> compilePath(QStringView sv)
{
    qCDebug(jsonPathLog) << "compilePath() sv=" << sv;
    using K = Token::Kind;
    QVector<Token> tokens;
    QVector<ContextFilterFn> contextFilters;
    QVector<FilterFn> filters;
    tokens.reserve(sv.count('.') + sv.count('[') + 4);

    // root
    if (sv.empty() || (sv[0] != u'$' && sv[0] != u'@'))
        return std::unexpected(Error::MissingRoot);
    tokens.append(Token{ K::Key, 0, {}, qt_hash(sv.first(1)),
                         sv.first(1).toString(), 0 });

    // must be followed by '.' or '[' unless path ends here
    if (sv.size() > 1 && sv[1] != u'.' && sv[1] != u'[')
        return std::unexpected(Error::UnexpectedAfterRoot);

    detail::KeyBuilder kb{tokens};

    // scan
    for (qsizetype pos = 1, n = sv.size(); pos < n; )
    {
        std::expected<qsizetype,Error> next =
            (sv[pos] == u'.') ? detail::parseDot    (pos, sv, kb, tokens)
          : (sv[pos] == u'[') ? detail::parseBracket(pos, sv, kb, tokens, contextFilters, filters)
          :                    detail::parseBare   (pos, sv, kb);

        if (!next) {
            qCDebug(jsonPathLog) << "compilePath: parser returned error" << static_cast<int>(next.error());
            return std::unexpected(next.error());
        }
        pos = *next;
    }
    return Compiled{ std::move(tokens), std::move(filters), std::move(contextFilters) };
}

// ──────────────────────────────────────────────────────────────────────
//  High-level compile function
// ──────────────────────────────────────────────────────────────────────
std::expected<CompilationResult, Error> compile(QStringView rawPath)
{
    qCDebug(jsonPathLog) << "compile() rawPath=" << rawPath;
    QString path = rawPath.toString();
    
    // Extract any trailing function → updates `path` and yields `func`
    FunctionType func = detectTrailingFunction(path);

    if (path.isEmpty())
        return std::unexpected(Error::EmptySegment);

    // Compile into tokens + filter list
    auto compiled = compilePath(path);
    if (!compiled) {
        qCDebug(jsonPathLog) << "compile: compilePath failed with error" << static_cast<int>(compiled.error());
        return std::unexpected(compiled.error());
    }

    return CompilationResult{
        func,
        std::move(*compiled)
    };
}

std::optional<Token> compileFilter(const QString& expr, QVector<FilterFn>& filters)
{
    // For backward compatibility, wrap context-aware compilation
    QVector<ContextFilterFn> contextFilters;
    auto result = json_query::json_path::compileContextFilter(expr, contextFilters, filters);
    
    if (result && result->contextFilterId < contextFilters.size()) {
        // Extract the context filter and create a regular filter wrapper
        const auto& contextFilter = contextFilters[result->contextFilterId];
        filters.push_back([contextFilter](const QJsonValue& currentNode) -> bool {
            // Call context filter with dummy root (backward compatibility)
            return contextFilter(currentNode, QJsonValue{});
        });
        
        // Update token to use regular filter
        result->filterId = filters.size() - 1;
        result->contextFilterId = SIZE_MAX;
    }
    
    return result;
}

} // namespace json_query::json_path
