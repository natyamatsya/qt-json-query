#include "JSONPath.hpp"

// ──────────────────────────────────────────────────────────────────────
//  Helpers local to this TU
// ──────────────────────────────────────────────────────────────────────
namespace
{
    using Slice = JSONPath::Slice;
    using Token = JSONPath::Token;
    using Error = JSONPath::Error;

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
        if (key.isEmpty()) return std::unexpected(Error::EmptySegment);
        QString s = key.toString();
        tgt.append(Token{ Token::Kind::Key, 0, {}, qt_hash(s), std::move(s), 0 });
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

// Friend‑level wrapper so rule lambdas can call compileFilter  ----------------
inline std::optional<Token>
callCompileFilter(const QString& expr, QVector<JSONPath::FilterFn>& f)
{
    return JSONPath::compileFilter(expr, f);        // still private
}

// A small façade so every rule can emit tokens consistently
    // BracketSink now carries *filters* as well
    struct BracketSink {
    QVector<Token>&   tk;
    KeyBuilder&       kb;
    QVector<JSONPath::FilterFn>& filters;

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
        if (content == u"*") { out.wild(); return {}; }
        return std::nullopt;
    },

    // 2.  ?(filter)  -----------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.isEmpty() && content.front()==u'?') {
            auto tk = callCompileFilter(content.mid(1).toString(), out.filters);
            if (!tk) return Error::UnsupportedFilter;
            out.pushFilter(*tk);
            return {};
        }
        return std::nullopt;
    },

    // 3.  slice      -----------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (content.contains(u':')) {
            out.slice(makeSlice(content));
            return {};
        }
        return std::nullopt;
    },

    // 4.  quoted key -----------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        if (!content.isEmpty() &&
           (content.front()==u'\'' || content.front()==u'\"'))
        {
            const qsizetype qEnd = content.lastIndexOf(content.front());
            if (qEnd <= 0) return Error::UnmatchedQuote;
            if (auto r = out.key(content.sliced(1,qEnd-1)); !r)
                return r.error();
            return {};
        }
        return std::nullopt;
    },

    // 5.  numeric index --------------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        bool ok=false; int idx = QString(content).toInt(&ok);
        if (ok) { out.index(idx); return {}; }
        return std::nullopt;
    },

    // 6.  fallback raw key -----------------------------------------
    [](QStringView content, BracketSink& out)->std::optional<Error> {
        const QString wrapped = "[" + QString(content) + "]";
        if (auto r = out.key(wrapped); !r) return r.error();
        return {};
    }
}};

// ------------------------------------------------------------------
// parseBracket dispatcher
// ------------------------------------------------------------------
[[nodiscard]] std::expected<qsizetype, Error>
parseBracket(qsizetype pos,
             QStringView          sv,
             KeyBuilder&          kb,
             QVector<Token>&      tokens,
             QVector<JSONPath::FilterFn>&   filters)
    {
        using K = Token::Kind;

        // 1. find the matching ’]’ -------------------------------------------------
        const qsizetype n      = sv.size();
        const qsizetype startB = ++pos;                 // skip initial '['

        int depth = 1;
        while (pos < n && depth) {
            if      (sv[pos] == u'[') ++depth;
            else if (sv[pos] == u']') --depth;
            ++pos;
        }
        if (depth) return std::unexpected(Error::UnmatchedBracket);

        const QStringView content = sv.sliced(startB, pos - startB - 1);

        // 2. run rule table until one rule adds something --------------------------
        BracketSink sink{tokens, kb, filters};

        for (auto&& rule : BR_RULES)
        {
            const std::size_t tokBefore   = tokens .size();
            const std::size_t filtBefore  = filters.size();

            if (auto err = rule(content, sink))               // matched but errored
                return std::unexpected(*err);

            // did this rule append at least one token or filter?
            if (tokens.size() != tokBefore || filters.size() != filtBefore)
                return pos;                                   // handled → stop
        }

        // Fallback rule in BR_RULES always handles the segment, so we should never
        // reach this line. If we do, return a generic syntax error.
        return std::unexpected(Error::InvalidIndex);
    }

// bare‑name parser: replace kb.pushKey → kb.push
[[nodiscard]] std::expected<qsizetype,Error>
parseBare(qsizetype pos, QStringView sv, KeyBuilder& kb)
{
    qsizetype n = sv.size(), start = pos;
    while (pos < n && sv[pos] != u'.' && sv[pos] != u'[') ++pos;
    if (auto r = kb.push(sv.sliced(start, pos - start)); !r)
        return std::unexpected(r.error());
    return pos;
}
} // namespace detail

std::expected<JSONPath::Compiled, Error>
JSONPath::compilePath(QStringView sv)
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
