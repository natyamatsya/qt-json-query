#include "json-query/json-path/JSONPathCompile.hpp"

#include <iostream>
#include <QDebug>
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"

#include "json-query/json-path/internal/QtHash.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include "json-query/json-path/JSONPathFilter.hpp"  // For compileFilter implementation

#include <limits>
#include <ctre.hpp>

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
        static constexpr auto integer_literal_pattern = ctre::match<"^(?:0|-[1-9][0-9]*|[1-9][0-9]*)$">;
        
        if (!integer_literal_pattern(content.toString().toStdString())) {
            qCDebug(jsonPathLog) << "isValidIndexLiteral(" << content << ") invalid integer literal";
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

    std::expected<void, Error> key(QString key, bool allow=false) { 
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

// bare‑name parser: replace kb.pushKey → kb.push
[[nodiscard]] std::expected<qsizetype,Error>
parseBare(qsizetype pos, QStringView sv, KeyBuilder& kb)
{
    qCDebug(jsonPathLog) << "parseBare pos=" << pos;
    qsizetype start = pos;
    const qsizetype n = sv.size();
    while (pos < n && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
    if (pos == start) return std::unexpected(Error::EmptySegment);
    QStringView key = sv.sliced(start, pos - start);
    if (auto r = kb.push(key.toString()); !r)
        return std::unexpected(r.error());
    return pos;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TableGen-Inspired Declarative Bracket Rule System
// ═══════════════════════════════════════════════════════════════════════════════

// Rule handler function type with enhanced error semantics
using BracketRuleHandler = std::function<std::expected<void, Error>(QStringView, BracketSink&)>;

// Rule matcher function type for pattern detection
using BracketRuleMatcher = std::function<bool(QStringView)>;

// Declarative rule metadata structure
struct BracketRuleMetadata {
    const char* name;                    // Human-readable rule name
    int priority;                        // Higher priority = checked first
    BracketRuleMatcher matcher;          // Pattern detection function
    BracketRuleHandler handler;          // Processing function
    const char* description;             // Documentation string
};

// Forward declaration for recursive calls
class BracketRuleDispatcher;

// ═══════════════════════════════════════════════════════════════════════════════
// Rule Matcher Functions (Pattern Detection)
// ═══════════════════════════════════════════════════════════════════════════════

namespace matchers {

static bool matchesUnionComma(QStringView content) {
    return content.contains(u',');
}

static bool matchesWildcard(QStringView content) {
    return content == u"*";
}

static bool matchesSingleIndex(QStringView content) {
    return isValidIndexLiteral(content);
}

static bool matchesIndexList(QStringView content) {
    if (!content.contains(u',')) return false;
    // Quick check: no quotes or slice colons
    if (content.contains(u'\'') || content.contains(u'"') || content.contains(u':'))
        return false;
    
    const auto parts = content.split(u',');
    for (QStringView p : parts) {
        QStringView t = p.trimmed();
        if (!isValidIndexLiteral(t)) return false;
    }
    return true;
}

static bool matchesSlice(QStringView content) {
    return content.contains(u':');
}

static bool matchesFilterWithParens(QStringView content) {
    return content.startsWith(u"?(") && content.endsWith(u')');
}

static bool matchesFilterWithoutParens(QStringView content) {
    return content.startsWith(u'?') && !content.startsWith(u"?(");
}

static bool matchesPlaceholder(QStringView content) {
    if (!content.contains(u'?')) return false;
    // verify pattern consists only of '?' separated by commas/spaces
    for (QStringView part : content.split(u',')) {
        if (part.trimmed() != u"?") return false;
    }
    return true;
}

static bool matchesQuotedKey(QStringView content) {
    if (content.size() < 2) return false;
    QChar q = content.front();
    return (q == u'\'' || q == u'"') && content.back() == q;
}

static bool matchesUnquotedKey(QStringView /*content*/) {
    return false; // RFC 9535 forbids unquoted string literals inside []
}

} // namespace matchers

// ═══════════════════════════════════════════════════════════════════════════════
// Rule Handler Functions (Processing Logic)
// ═══════════════════════════════════════════════════════════════════════════════

namespace handlers {

static std::expected<void, Error> handleUnionComma(QStringView content, BracketSink& out);

static std::expected<void, Error> handleWildcard(QStringView /*content*/, BracketSink& out) {
    out.wild();
    return {};
}

static std::expected<void, Error> handleSingleIndex(QStringView content, BracketSink& out) {
    qCDebug(jsonPathLog) << "BR_RULE index-single check" << content.toString();
    
    bool ok = false;
    qlonglong val = content.toLongLong(&ok);
    if (!ok) return std::unexpected(Error::InvalidSlice);
    
    int idx = (val > std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
             : (val < std::numeric_limits<int>::min()) ? std::numeric_limits<int>::min()
             : static_cast<int>(val);
    
    qCDebug(jsonPathLog) << "  emitting index token" << idx;
    out.index(idx);
    return {};
}

static std::expected<void, Error> handleIndexList(QStringView content, BracketSink& out) {
    qCDebug(jsonPathLog) << "BR_RULE index-list raw" << content.toString();
    
    const auto parts = content.split(u',');
    for (QStringView p : parts) {
        QStringView t = p.trimmed();
        bool ok = false;
        qlonglong val = t.toLongLong(&ok);
        if (!ok) return std::unexpected(Error::InvalidSlice);
        
        int idx = (val > std::numeric_limits<int>::max()) ? std::numeric_limits<int>::max()
                 : (val < std::numeric_limits<int>::min()) ? std::numeric_limits<int>::min()
                 : static_cast<int>(val);
        
        qCDebug(jsonPathLog) << "  list element emit" << idx;
        out.index(idx);
    }
    return {};
}

static std::expected<void, Error> handleSlice(QStringView content, BracketSink& out) {
    if (auto maybe = makeSlice(content)) {
        out.slice(*maybe);
        return {};
    }
    // Return error instead of nullopt to allow other rules to try
    return std::unexpected(Error::InvalidSlice);
}

static std::expected<void, Error> handleFilterWithParens(QStringView content, BracketSink& out) {
    QString expr = content.sliced(2, content.size() - 3).toString();
    
    if (auto tok = json_query::json_path::compileContextFilter(expr, out.contextFilters, out.filters)) {
        out.pushFilter(*tok);
        return {};
    }
    return std::unexpected(Error::UnsupportedFilter);
}

static std::expected<void, Error> handleFilterWithoutParens(QStringView content, BracketSink& out) {
    QStringView exprView = content.sliced(1).trimmed();
    if (exprView.isEmpty()) return std::unexpected(Error::UnsupportedFilter);
    
    QString expr = exprView.toString();
    qCDebug(jsonPathLog) << "BR_RULE filter-no-parens: calling compileContextFilter with expr:" << expr;
    
    if (auto tok = json_query::json_path::compileContextFilter(expr, out.contextFilters, out.filters)) {
        qCDebug(jsonPathLog) << "BR_RULE filter-no-parens: compileContextFilter succeeded";
        out.pushFilter(*tok);
        return {};
    }
    
    qCDebug(jsonPathLog) << "BR_RULE filter-no-parens: compileContextFilter failed";
    return std::unexpected(Error::UnsupportedFilter);
}

static std::expected<void, Error> handlePlaceholder(QStringView content, BracketSink& out) {
    // For each placeholder, create a no-op filter token that will be
    // resolved later (currently always-true).
    for ([[maybe_unused]] QStringView _ : content.split(u',')) {
        auto alwaysTrue = [](const QJsonValue& /*currentNode*/, const QJsonValue& /*root*/) { return true; };
        out.contextFilters.append(alwaysTrue);
        Token t{Token::Kind::Filter};
        t.contextFilterId = out.contextFilters.size() - 1;
        out.pushFilter(t);
    }
    return {};
}

static std::expected<void, Error> handleQuotedKey(QStringView content, BracketSink& out) {
    QStringView unquoted = content.sliced(1, content.size() - 2);
    QChar q = content.front();
    QuoteStyle st = (q == u'\'') ? QuoteStyle::Single : QuoteStyle::Double;
    
    if (!isValidQuotedKey(unquoted, st)) {
        return std::unexpected(Error::InvalidSlice);
    }
    
    QString unesc = unescapeQuotedKey(unquoted);
    return out.key(unesc, true).transform_error([](Error e) { return e; });
}

static std::expected<void, Error> handleUnquotedKey(QStringView /*content*/, BracketSink& /*out*/) {
    return std::unexpected(Error::UnsupportedFilter); // RFC 9535 forbids this
}

} // namespace handlers

// ═══════════════════════════════════════════════════════════════════════════════
// TableGen-Inspired Declarative Rule Table
// ═══════════════════════════════════════════════════════════════════════════════

class BracketRuleDispatcher {
private:
    // Initialize rules at runtime to avoid constexpr issues
    static std::vector<BracketRuleMetadata> createRules() {
        return {
            {
                .name = "union_comma",
                .priority = 1000,
                .matcher = matchers::matchesUnionComma,
                .handler = handlers::handleUnionComma,
                .description = "Comma-separated union selectors (e.g., 'a,b,1,2')"
            },
            {
                .name = "wildcard",
                .priority = 900,
                .matcher = matchers::matchesWildcard,
                .handler = handlers::handleWildcard,
                .description = "Wildcard selector (*)"
            },
            {
                .name = "index_list",
                .priority = 850,
                .matcher = matchers::matchesIndexList,
                .handler = handlers::handleIndexList,
                .description = "Comma-separated index list (e.g., '1,2,3')"
            },
            {
                .name = "single_index",
                .priority = 800,
                .matcher = matchers::matchesSingleIndex,
                .handler = handlers::handleSingleIndex,
                .description = "Single array index (e.g., '123')"
            },
            {
                .name = "slice",
                .priority = 700,
                .matcher = matchers::matchesSlice,
                .handler = handlers::handleSlice,
                .description = "Array slice (e.g., '1:3:2')"
            },
            {
                .name = "filter_with_parens",
                .priority = 600,
                .matcher = matchers::matchesFilterWithParens,
                .handler = handlers::handleFilterWithParens,
                .description = "Filter expression with parentheses (e.g., '?(@.a == 1)')"
            },
            {
                .name = "filter_without_parens",
                .priority = 550,
                .matcher = matchers::matchesFilterWithoutParens,
                .handler = handlers::handleFilterWithoutParens,
                .description = "Filter expression without parentheses (e.g., '?@.a == 1')"
            },
            {
                .name = "placeholder",
                .priority = 500,
                .matcher = matchers::matchesPlaceholder,
                .handler = handlers::handlePlaceholder,
                .description = "Placeholder filter (e.g., '?' or '?,?')"
            },
            {
                .name = "quoted_key",
                .priority = 400,
                .matcher = matchers::matchesQuotedKey,
                .handler = handlers::handleQuotedKey,
                .description = "Quoted string key (e.g., \"'key'\" or '\"key\"')"
            },
            {
                .name = "unquoted_key",
                .priority = 100,
                .matcher = matchers::matchesUnquotedKey,
                .handler = handlers::handleUnquotedKey,
                .description = "Unquoted key (forbidden by RFC 9535)"
            }
        };
    }

public:
    // Get rules (initialized once)
    static const std::vector<BracketRuleMetadata>& getRules() {
        static const auto rules = createRules();
        return rules;
    }

    // Main dispatch function using declarative rule table
    static std::expected<void, Error> dispatch(QStringView content, BracketSink& sink) {
        qCDebug(jsonPathLog) << "BracketRuleDispatcher::dispatch content=" << content.toString();
        
        // Apply rules in priority order using monadic error handling
        for (const auto& rule : getRules()) {
            qCDebug(jsonPathLog) << "Trying rule:" << rule.name << "(priority:" << rule.priority << ")";
            
            if (rule.matcher(content)) {
                qCDebug(jsonPathLog) << "Rule" << rule.name << "matched, applying handler";
                auto result = rule.handler(content, sink);
                
                if (result) {
                    qCDebug(jsonPathLog) << "Rule" << rule.name << "succeeded";
                    return {};
                } else {
                    qCDebug(jsonPathLog) << "Rule" << rule.name << "failed with error" << static_cast<int>(result.error());
                    return result;
                }
            } else {
                qCDebug(jsonPathLog) << "Rule" << rule.name << "did not match";
            }
        }
        
        qCDebug(jsonPathLog) << "No rules matched, returning UnsupportedFilter";
        return std::unexpected(Error::UnsupportedFilter);
    }

    // Helper for union processing to avoid recursion
    static std::expected<void, Error> processSegmentExcludingUnion(QStringView content, BracketSink& sink) {
        // Apply all rules except union_comma to prevent recursion
        for (const auto& rule : getRules()) {
            if (std::string_view(rule.name) == "union_comma") continue; // Skip union rule
            
            if (rule.matcher(content)) {
                return rule.handler(content, sink);
            }
        }
        return std::unexpected(Error::UnsupportedFilter);
    }

    // Utility function to get rule metadata for debugging/documentation
    static const BracketRuleMetadata* findRuleByName(const char* name) {
        for (const auto& rule : getRules()) {
            if (std::string_view(rule.name) == name) {
                return &rule;
            }
        }
        return nullptr;
    }
};

// Now implement the union handler that was forward declared
namespace handlers {

static std::expected<void, Error> handleUnionComma(QStringView content, BracketSink& out) {
    // Parse comma-separated selectors using multi-part splitting
    using json_query::json_path::detail::splitTopLevelMultiple;
    auto parts = splitTopLevelMultiple(content, QLatin1StringView(","));
    if (!parts) return std::unexpected(Error::UnsupportedFilter);

    // Helper lambda to route a single segment through existing rules (skip union rule to prevent recursion)
    auto compileOne = [&](QStringView seg) -> std::expected<void, Error> {
        // Case A: segment starts with '?'  → standalone filter expression
        if (seg.startsWith(u'?')) {
            QString expr = QString(seg.mid(1)).trimmed();
            if (expr.isEmpty()) return {};
            if (auto tok = json_query::json_path::compileContextFilter(expr, out.contextFilters, out.filters)) {
                out.pushFilter(*tok);
                return {};
            }
            return std::unexpected(Error::UnsupportedFilter);
        }

        // Case B: delegate to other bracket rules using dispatcher
        return BracketRuleDispatcher::processSegmentExcludingUnion(seg, out);
    };

    // Process all parts with monadic error handling
    for (const QString& part : *parts) {
        QStringView seg = QStringView(part).trimmed();
        if (auto result = compileOne(seg); !result) {
            return result;
        }
    }
    return {};
}

} // namespace handlers

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

    // Use TableGen dispatcher instead of manual rule loop
    return BracketRuleDispatcher::dispatch(content, sink)
        .transform([pos]() { return pos + 1; })
        .transform_error([](Error e) { return e; });
}

} // namespace detail

// ──────────────────────────────────────────────────────────────────────
//  detectTrailingFunction - moved from JSONPath
// ──────────────────────────────────────────────────────────────────────
json_query::json_path::FunctionType detectTrailingFunction(QString& path)
{
    using enum json_query::json_path::FunctionType;

    static const QPair<QString, json_query::json_path::FunctionType> table[] = {
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
std::expected<json_query::json_path::Compiled, json_query::json_path::Error> compilePath(QStringView sv)
{
    qCDebug(json_query::json_path::jsonPathLog) << "compilePath() sv=" << sv;
    using K = json_query::json_path::Token::Kind;
    QVector<json_query::json_path::Token> tokens;
    QVector<json_query::json_path::ContextFilterFn> contextFilters;
    QVector<json_query::json_path::FilterFn> filters;
    json_query::json_path::detail::KeyBuilder kb{tokens};

    if (sv.isEmpty() || sv[0] != u'$')
        return std::unexpected(json_query::json_path::Error::MissingRoot);
    tokens.append(json_query::json_path::Token{ K::Key, 0, {}, qt_hash(sv.first(1)),
                                                sv.first(1).toString() });

    if (sv.size() > 1 && sv[1] != u'.' && sv[1] != u'[')
        return std::unexpected(json_query::json_path::Error::UnexpectedAfterRoot);

    // C++23 Monadic Chain - Functional composition for parsing loop
    // Transform imperative loop into elegant monadic fold operation
    struct ParseState {
        qsizetype pos;
        QVector<json_query::json_path::Token>& tokens;
        QVector<json_query::json_path::ContextFilterFn>& contextFilters;
        QVector<json_query::json_path::FilterFn>& filters;
        json_query::json_path::detail::KeyBuilder& kb;
        QStringView sv;
    };
    
    ParseState state{1, tokens, contextFilters, filters, kb, sv};
    
    // Monadic fold: repeatedly apply parser until end of input or error
    return std::expected<ParseState, json_query::json_path::Error>{std::move(state)}
        .and_then([](ParseState&& state) -> std::expected<ParseState, json_query::json_path::Error> {
            // Recursive monadic parser application
            std::function<std::expected<ParseState, json_query::json_path::Error>(ParseState&&)> parseNext = 
                [&parseNext](ParseState&& currentState) -> std::expected<ParseState, json_query::json_path::Error> {
                
                // Base case: reached end of input
                if (currentState.pos >= currentState.sv.size()) {
                    return std::move(currentState);
                }
                
                // Apply appropriate parser based on current character
                auto nextPosResult = 
                    (currentState.sv[currentState.pos] == u'.' && 
                     currentState.pos + 1 < currentState.sv.size() && 
                     currentState.sv[currentState.pos + 1] == u'.') ? 
                        // Handle descendant segment (..) directly
                        [&currentState]() -> std::expected<qsizetype, json_query::json_path::Error> {
                            qCDebug(json_query::json_path::jsonPathLog) << "compilePath: found descendant segment (..) at pos=" << currentState.pos;
                            currentState.tokens.append(json_query::json_path::Token{json_query::json_path::Token::Kind::Recursive});
                            qsizetype newPos = currentState.pos + 2;
                            if (newPos >= currentState.sv.size()) {
                                return std::unexpected(json_query::json_path::Error::TrailingRecursive);
                            }
                            return newPos;
                        }()
                  : (currentState.sv[currentState.pos] == u'*') ?
                        // Handle wildcard directly
                        [&currentState]() -> std::expected<qsizetype, json_query::json_path::Error> {
                            qCDebug(json_query::json_path::jsonPathLog) << "compilePath: found wildcard (*) at pos=" << currentState.pos;
                            currentState.tokens.append(json_query::json_path::Token{json_query::json_path::Token::Kind::Wildcard});
                            return currentState.pos + 1;
                        }()
                  : (currentState.sv[currentState.pos] == u'.') ? 
                        json_query::json_path::detail::parseDot(currentState.pos, currentState.sv, currentState.kb, currentState.tokens)
                  : (currentState.sv[currentState.pos] == u'[') ? 
                        json_query::json_path::detail::parseBracket(currentState.pos, currentState.sv, currentState.kb, currentState.tokens, 
                                           currentState.contextFilters, currentState.filters)
                  :     json_query::json_path::detail::parseBare(currentState.pos, currentState.sv, currentState.kb);
                
                // Monadic composition: chain parser result with recursive call
                return nextPosResult
                    .and_then([&parseNext, currentState = std::move(currentState)](qsizetype nextPos) mutable -> std::expected<ParseState, json_query::json_path::Error> {
                        currentState.pos = nextPos;
                        return parseNext(std::move(currentState));
                    })
                    .or_else([](json_query::json_path::Error error) -> std::expected<ParseState, json_query::json_path::Error> {
                        qCDebug(json_query::json_path::jsonPathLog) << "compilePath: parser returned error" << static_cast<int>(error);
                        return std::unexpected(error);
                    });
            };
            
            return parseNext(std::move(state));
        })
        .transform([](ParseState&& finalState) -> json_query::json_path::Compiled {
            qCDebug(json_query::json_path::jsonPathLog) << "compilePath: monadic parsing completed successfully";
            return json_query::json_path::Compiled{ 
                std::move(finalState.tokens), 
                std::move(finalState.filters), 
                std::move(finalState.contextFilters) 
            };
        });
}

// ──────────────────────────────────────────────────────────────────────
//  High-level compile function
// ──────────────────────────────────────────────────────────────────────
std::expected<json_query::json_path::CompilationResult, json_query::json_path::Error> compile(QStringView rawPath)
{
    qCDebug(json_query::json_path::jsonPathLog) << "compile() rawPath=" << rawPath;
    QString path = rawPath.toString();
    
    // Extract any trailing function → updates `path` and yields `func`
    json_query::json_path::FunctionType func = json_query::json_path::detectTrailingFunction(path);

    if (path.isEmpty())
        return std::unexpected(json_query::json_path::Error::EmptySegment);

    // C++23 Monadic Chain - Elegant error composition without manual checks!
    return json_query::json_path::compilePath(path)
        .transform([func](json_query::json_path::Compiled&& compiled) -> json_query::json_path::CompilationResult {
            qCDebug(json_query::json_path::jsonPathLog) << "compile: compilePath succeeded";
            return json_query::json_path::CompilationResult{
                func,
                std::move(compiled)
            };
        })
        .or_else([](json_query::json_path::Error error) -> std::expected<json_query::json_path::CompilationResult, json_query::json_path::Error> {
            qCDebug(json_query::json_path::jsonPathLog) << "compile: compilePath failed with error" << static_cast<int>(error);
            return std::unexpected(error);
        });
}

} // namespace json_query::json_path
