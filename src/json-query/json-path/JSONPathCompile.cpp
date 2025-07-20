#include "json-query/json-path/JSONPathCompile.hpp"

#include <iostream>

#include "json-query/json-path/internal/QtHash.hpp"
#include "../../../include/json-query/utils/JSONQueryUtils.hpp"
#include "json-query/json-path/JSONPathFilter.hpp"  // For compileFilter implementation

#include <limits>

namespace json_query::json_path
{

// ──────────────────────────────────────────────────────────────────────
//  Helpers local to this compilation unit
// ──────────────────────────────────────────────────────────────────────
namespace
{
    // Parse start:end:step  (parts may be empty) --------------------------------
    [[nodiscard]] static inline Slice makeSlice(QStringView v)
    {
        auto toInt = [](QStringView part, int def){
            if(part.isEmpty()) return def;
            bool ok=false; int x=part.toInt(&ok); return ok?x:def;
        };

        const auto parts = v.split(u':');
        const QStringView a = parts.size()>0 ? parts[0].trimmed() : QStringView{};
        const QStringView b = parts.size()>1 ? parts[1].trimmed() : QStringView{};
        const QStringView c = parts.size()>2 ? parts[2].trimmed() : QStringView{};

        const int start = toInt(a, 0);
        const int end   = toInt(b, std::numeric_limits<int>::max());
        const int step  = toInt(c, 1);
        return {start,end,step};
    }
} // namespace

namespace detail {

// ────────────────────────────────────────────────────────────────
// 1. pushKey helper (renamed)
// ────────────────────────────────────────────────────────────────
struct KeyBuilder {
    QVector<Token>& tgt;

    std::expected<void,Error> push(QStringView key, bool allowSpace = false)
    {
        if (key.isEmpty())
            return std::unexpected(Error::EmptySegment);
        if (!allowSpace && key.contains(u' '))
            return std::unexpected(Error::BlankInKey);

        tgt.append(Token{Token::Kind::Key, 0, {}, qt_hash(key), key.toString()});
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
    if (auto r = kb.push(sv.sliced(start, pos - start)); !r)
        return std::unexpected(r.error());
    return pos;
}

// A small façade so every rule can emit tokens consistently
// BracketSink now carries *filters* as well
struct BracketSink {
    QVector<Token>&   tk;
    KeyBuilder&       kb;
    QVector<FilterFn>& filters;

    std::expected<void,Error> key(QStringView k, bool allow=false) { return kb.push(k, allow); }
    void keyList(const QVector<QStringView>& keys)
    {
        if (keys.isEmpty()) return;
        Token t;
        t.kind = Token::Kind::KeyList;
        QString joined;
        for (qsizetype i=0;i<keys.size();++i) {
            if (i) joined.push_back(u'\n');
            joined.append(QString(keys[i]));
        }
        t.key = std::move(joined);
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
                QVector<QStringView> keys;
                for (qsizetype i=0;i<=content.size();++i)
                {
                    if (i==content.size() || (content[i]==u',' && quoteCount==0))
                    {
                        QStringView part = content.sliced(start, i-start).trimmed();
                        if (part.isEmpty()) return Error::EmptySegment;
                        if (part.size()<2) return std::optional<Error>{};
                        QChar q=part.front();
                        if ((q!=u'\'' && q!=u'\"') || part.back()!=q) return std::optional<Error>{};
                        QStringView key = part.sliced(1, part.size()-2);
                        keys.append(key);
                        start = i+1;
                    } else if (content[i]==u'\'' || content[i]==u'\"') quoteCount ^=1;
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
        out.slice(makeSlice(content));
        return Error::Ok;
    },

    // 4.  ?(...) ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.startsWith(u"?(") || !content.endsWith(u')'))
            return std::nullopt;

        QString expr = content.sliced(2, content.size() - 3).toString();
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
        if (auto r = out.key(unquoted, true); !r)
            return r.error();
        return Error::Ok;
    },

    // 7.  key (unquoted) -------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (content.contains(u'\'') || content.contains(u'"') || content.contains(u':') || content.contains(u','))
            return std::nullopt; // avoid other kinds
        if (auto r = out.key(content); !r)
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
    const qsizetype n = sv.size();
    if (++pos >= n) return std::unexpected(Error::UnmatchedBracket);

    qsizetype start = pos;
    int bracketLevel = 0;
    while (pos < n) {
        if (sv[pos] == u'[') {
            ++bracketLevel;
        } else if (sv[pos] == u']') {
            if (bracketLevel == 0) break; // Found the matching closing bracket
            --bracketLevel;
        } else if (sv[pos] == u'\'' || sv[pos] == u'"') {
            QChar quote = sv[pos++];
            while (pos < n && sv[pos] != quote) ++pos;
            if (pos >= n) return std::unexpected(Error::UnmatchedQuote);
        }
        ++pos;
    }
    if (pos >= n) return std::unexpected(Error::UnmatchedBracket);

    QStringView raw = sv.sliced(start, pos - start);
    // trim leading/trailing whitespace
    qsizetype l=0, r=raw.size();
    while (l<r && raw[l].isSpace()) ++l;
    while (r>l && raw[r-1].isSpace()) --r;
    QStringView content = raw.sliced(l, r - l);

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
    if (auto r = kb.push(sv.sliced(start, pos - start)); !r)
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
