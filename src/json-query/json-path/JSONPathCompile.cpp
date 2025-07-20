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
    // Parse [start:end:step]   (empty parts get defaults)
    [[nodiscard]] static inline Slice makeSlice(QStringView v)
    {
        auto readInt = [](QStringView part, int def) {
            if (part.isEmpty()) return def;
            bool ok = false;  int x = part.toInt(&ok);
            return ok ? x : def;
        };

        const qsizetype c1 = v.indexOf(u':');
        const qsizetype c2 = v.indexOf(u':', c1 + 1);

        const int start = readInt(v.sliced(1, c1 - 1),                  0);
        const int end   = readInt(c2 == -1 ? v.sliced(c1 + 1, v.size()-c1-2)
                                           : v.sliced(c1 + 1, c2 - c1 - 1),
                                  std::numeric_limits<int>::max());
        const int step  = c2 == -1 ? 1
                                   : readInt(v.sliced(c2 + 1, v.size()-c2-2), 1);
        return { start, end, step };
    }
} // namespace

namespace detail {

// ────────────────────────────────────────────────────────────────
// 1. pushKey helper (renamed)
// ────────────────────────────────────────────────────────────────
struct KeyBuilder {
    QVector<Token>& tgt;

    std::expected<void,Error> push(QStringView key)
    {
        if (key.isEmpty())
            return std::unexpected(Error::EmptySegment);

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
    if (nxt == u'.') { tokens.append(Token{Token::Kind::Recursive}); ++pos; return pos; }
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

    std::expected<void,Error> key(QStringView k) { return kb.push(k); }
    void wild()                 { tk.append(Token{Token::Kind::Wildcard}); }
    void slice(const Slice& s)   { tk.append(Token{Token::Kind::Slice,0,s,0u}); }
    void index(int i)            { tk.append(Token{Token::Kind::Index,i}); }
    void pushFilter(const Token& t){ tk.append(t); }
};

// A helper to iterate over the content and run rules.
using BrRule = std::function<std::optional<Error>(QStringView, BracketSink&)>;

static const std::array<BrRule,6> BR_RULES = {{
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

    // 3.  1:3:2  ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.contains(u':')) return std::nullopt;
        out.slice(makeSlice(content));
        return Error::Ok;
    },

    // 4.  'key'  ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (content.size() < 2) return std::nullopt;
        QChar q = content.front();
        if ((q != u'\'' && q != u'"') || content.back() != q)
            return std::nullopt;

        QStringView unquoted = content.sliced(1, content.size() - 2);
        if (auto r = out.key(unquoted); !r)
            return r.error();
        return Error::Ok;
    },

    // 5.  ?(...) ----------------------------------------------------
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

    // 6.  key    ----------------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
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

    QStringView content = sv.sliced(start, pos - start);
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
