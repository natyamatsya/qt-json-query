#include "json-query/json-path/JSONPathCompile.hpp"

#include <iostream>
#include <QDebug>
#include "json-query/json-path/JSONPathLog.hpp"

#include "json-query/json-path/internal/QtHash.hpp"
#include "../../../include/json-query/utils/JSONQueryUtils.hpp"
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

        auto strictParse = [&](QStringView part, std::optional<qsizetype>& out)->bool{
            part = part.trimmed();
            if (part.isEmpty()) {
                out.reset();
                return true; // omitted component
            }

            if (!re.matchView(part).hasMatch())
                return false; // not a valid integer literal per RFC

            // Fast path 64-bit conversion first.
            bool ok = false;
            const qlonglong v64 = part.toLongLong(&ok, 10);

            if (!ok)
                return false; // overflow beyond 64-bit – invalid selector (§4.2.1)

            // RFC 9535 §4.2.3: each literal MUST fit in signed-32-bit range.
            static constexpr qlonglong INT32_MIN_LL = static_cast<qlonglong>(std::numeric_limits<int>::min());
            static constexpr qlonglong INT32_MAX_LL = static_cast<qlonglong>(std::numeric_limits<int>::max());
            // If outside 32-bit range, treat as ±∞ sentinel by clamping to qsizetype min/max
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

        if (parts.size()>0 && !strictParse(parts[0].trimmed(), startOpt)) { qCDebug(jsonPathLog) << "strictParse failed for start"; return std::nullopt; }
        if (parts.size()>1 && !strictParse(parts[1].trimmed(), endOpt))   { qCDebug(jsonPathLog) << "strictParse failed for end"; return std::nullopt; }
        if (parts.size()>2 && !strictParse(parts[2].trimmed(), stepOpt))  { qCDebug(jsonPathLog) << "strictParse failed for step"; return std::nullopt; }

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
    [[nodiscard]] static bool isValidQuotedKey(QStringView key, QuoteStyle /*style*/)
    {
        // RFC 9535 allows empty string
        for (qsizetype i = 0; i < key.size(); ++i) {
            if (key[i] != u'\\') continue;
            if (i + 1 >= key.size()) return false; // dangling
            QChar esc = key[i + 1];
            // Accept standard JSON escapes and unicode
            if (QStringLiteral("\\\"'/bfnrtu").indexOf(esc) == -1)
                return false;
            if (esc == u'u') {
                if (i + 5 >= key.size()) return false;
                for (int k = 1; k <= 4; ++k) {
                    QChar h = key[i + k + 1];
                    if (!h.isDigit() && (h.toLower() < u'a' || h.toLower() > u'f'))
                        return false;
                }
                i += 5; // skip '\uXXXX'
            } else {
                ++i; // skip escape char
            }
        }
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
    if (nxt == u'.') {
        tokens.append(Token{Token::Kind::Recursive});
        ++pos;
        if (pos >= n) return std::unexpected(Error::TrailingRecursive);
        return pos;
    }
    if (nxt == u'*') { tokens.append(Token{Token::Kind::Wildcard }); ++pos; return pos; }

    qsizetype start = pos;
    while (pos < n && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
    if (auto r = kb.push(sv.sliced(start, pos - start).toString()); !r)
        return std::unexpected(r.error());
    return pos;
}

// A small façade so every rule can emit tokens consistently
// BracketSink now carries *filters* as well
struct BracketSink {
    QVector<Token>&   tk;
    KeyBuilder&       kb;
    QVector<FilterFn>& filters;

    std::expected<void,Error> key(QString key, bool allow=false) { return kb.push(key, allow); }
    void keyList(const QVector<QString>& keys)
    {
        if (keys.isEmpty()) return;

        Token t;
        t.kind = Token::Kind::KeyList;

        // Pack the keys into a single QString separated by '\n' so that the
        // evaluator can split them later without ambiguity.
        QStringList list;
        list.reserve(keys.size());
        for (const QString& k : keys)
            list.append(k);

        t.key = list.join(u"\n");
        tk.append(std::move(t));
    }
    void wild()                 { tk.append(Token{Token::Kind::Wildcard}); }
    void slice(const Slice& s)   { tk.append(Token{Token::Kind::Slice,0,s,0u}); }
    void index(int i)            { tk.append(Token{Token::Kind::Index,i}); }
    void pushFilter(const Token& t){ tk.append(t); }
};

// A helper to iterate over the content and run rules.
using BrRule = std::function<std::optional<Error>(QStringView, BracketSink&)>;

static const std::array<BrRule,9> BR_RULES = {{
    // 0.  'a', 'b'  -----------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        // Detect presence of comma outside quotes
        int quoteCount = 0;
        for (QChar c : content)
            if (c == u'\'' || c == u'"') quoteCount ^= 1; // toggle simple state
            else if (c == u',' && quoteCount == 0)
            {
                // Split by ',' respecting simple quote tracking (we only toggle, assumes same quote type)
                qsizetype start = 0;
                quoteCount = 0;
                QVector<QString> keys;
                for (qsizetype i=0;i<=content.size();++i)
                {
                    if (i==content.size() || (content[i]==u',' && quoteCount==0))
                    {
                        QStringView part = content.sliced(start, i-start).trimmed();
                        if (part.isEmpty()) return Error::EmptySegment;
                        if (part.size()<2) return std::optional<Error>{};
                        QChar q=part.front();
                        if ((q!=u'\'' && q!=u'\"') || part.back()!=q) return std::optional<Error>{};
                        QStringView keyView = part.sliced(1, part.size()-2);
                        QuoteStyle st = (q==u'\'') ? QuoteStyle::Single : QuoteStyle::Double;
                        if (!isValidQuotedKey(keyView, st)) return std::optional<Error>{};
                        QString unescaped = unescapeQuotedKey(keyView);
                        keys.append(unescaped);
                        start = i+1;
                    } else if (content[i]==u'\'' || content[i]==u'"') quoteCount ^=1;
                }
                out.keyList(keys);
                return Error::Ok;
            }
        return std::nullopt;
    },

    // 1.  *      ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (content != u"*") return std::nullopt;
        out.wild();
        return Error::Ok;
    },

    // 2.  123    ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        bool ok = false;
        int idx = content.toInt(&ok);
        if (!ok) return std::nullopt;
        out.index(idx);
        return Error::Ok;
    },

    // 2b.  1,2,3  --------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.contains(u',')) return std::nullopt;
        // Quick check: no quotes or slice colons
        if (content.contains(u'\'') || content.contains(u'"') || content.contains(u':'))
            return std::nullopt;
        const auto parts = content.split(u',');
        for (QStringView p : parts)
        {
            QStringView t = p.trimmed();
            bool ok=false; int idx = t.toInt(&ok);
            if (!ok) return std::nullopt; // not pure index list
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
            return Error::InvalidSlice;
        }
    },

    // 4.  ?(...) ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.startsWith(u"?(") || !content.endsWith(u')'))
            return std::nullopt;

        std::cout << "[bracket ?(] raw content=" << content.toString().toStdString() << std::endl;

        QString expr = content.sliced(2, content.size() - 3).toString();
        std::cout << "[bracket ?(] extracted expr=" << expr.toStdString() << std::endl;

        if (auto tok = json_query::json_path::compileFilter(expr, out.filters)) {
            out.pushFilter(*tok);
            return Error::Ok;
        }
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
            auto alwaysTrue = [](const QJsonValue&) { return true; };
            out.filters.append(alwaysTrue);
            Token t{Token::Kind::Filter};
            t.filterId = out.filters.size() - 1;
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

    // 7.  key (unquoted) -------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (content.contains(u'\'') || content.contains(u'"') || content.contains(u':') || content.contains(u','))
            return std::nullopt; // avoid other kinds
        if (auto r = out.key(content.toString()); !r)
            return r.error();
        return Error::Ok;
    }
}};

// ------------------------------------------------------------------
// parseBracket dispatcher
// ------------------------------------------------------------------
std::expected<qsizetype, Error> parseBracket(qsizetype pos,
         QStringView          sv,
         KeyBuilder&          kb,
         QVector<Token>&      tokens,
         QVector<FilterFn>&   filters)
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

    BracketSink sink{tokens, kb, filters};

    for (auto& rule : BR_RULES) {
        if (auto err = rule(content, sink)) {
            if (*err != Error::Ok) return std::unexpected(*err);
            return pos + 1;
        }
    }
    return std::unexpected(Error::UnsupportedFilter);
}

// bare‑name parser: replace kb.pushKey → kb.push
std::expected<qsizetype,Error> parseBare(qsizetype pos, QStringView sv, KeyBuilder& kb)
{
    qsizetype start = pos;
    while (pos < sv.size() && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
    if (pos == start) return std::unexpected(Error::EmptySegment);
    if (auto r = kb.push(sv.sliced(start, pos - start).toString()); !r)
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
          : (sv[pos] == u'[') ? detail::parseBracket(pos, sv, kb, tokens, filters)
          :                    detail::parseBare   (pos, sv, kb);

        if (!next) return std::unexpected(next.error());
        pos = *next;
    }
    return Compiled{ std::move(tokens), std::move(filters) };
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
    if (!compiled)
        return std::unexpected(compiled.error());

    return CompilationResult{
        func,
        std::move(*compiled)
    };
}

} // namespace json_query::json_path
