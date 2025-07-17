#pragma once

// ────────────────────────────── Qt
#include <QJsonDocument>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringView>
#include <QVector>

// ────────────────────────────── STL / C++23
#include <expected>
#include <functional>
#include <optional>
#include <variant>
#include <cstdint>

// ────────────────────────────── Project
#include "JSONQueryUtils.hpp"
#include "json-path/PathParser.hpp"
#include "JSONPointer.hpp"

#include <ctre.hpp>

using namespace Qt::StringLiterals;

namespace json_query
{
// ======================================================================
//  Compact, pre-decoded token layout
// ======================================================================
struct Slice { qsizetype start{}, end{}, step{}; };

struct Token {
    enum class Kind : quint8 {
        Key, Index, Slice, Wildcard, Recursive, Filter
    };
    Kind          kind {Kind::Key};
    qsizetype     index{};          // for Kind::Index
    Slice         slice{};          // for Kind::Slice
    quint32       hash{};           // cached object-key hash
    QString       key{};            // for Kind::Key / Filter
    std::size_t   filterId{};       // index into filter table
};

// ------------------------------------------------------------------
//  Parser / compiler error codes
// ------------------------------------------------------------------
enum class Error : std::uint8_t {
    Ok = 0,               // not used in expected<T,E>
    MissingRoot,
    TrailingDot,
    EmptySegment,
    UnmatchedBracket,
    UnmatchedQuote,
    UnsupportedFilter,
    InvalidSlice,
    InvalidIndex
};

[[nodiscard]] inline constexpr std::string_view
toString(Error e) noexcept
{
    using enum Error;
    switch (e) {
        case MissingRoot      : return "path must start with '$' or '@'";
        case TrailingDot      : return "trailing '.'";
        case EmptySegment     : return "empty property segment";
        case UnmatchedBracket : return "unmatched '['";
        case UnmatchedQuote   : return "unmatched quote inside [...]";
        case UnsupportedFilter: return "unsupported filter expression";
        case InvalidSlice     : return "bad array slice";
        case InvalidIndex     : return "bad numeric index";
        default               : return "unknown error";
    }
}

using FilterFn = std::function<bool (const QJsonValue&)>;

} // namespace json_query

class JSONPath;

namespace detail
{
    template<json_query::Token::Kind K>
    QJsonArray eval(const JSONPath&,
                    const json_query::Token&,
                    const QJsonValue&);
}

namespace detail {
    std::optional<json_query::Token> parseOr      (QString, QVector<json_query::FilterFn>&);
    std::optional<json_query::Token> parseAnd     (QString, QVector<json_query::FilterFn>&);
    std::optional<json_query::Token> parseIn      (QString, QVector<json_query::FilterFn>&);
    std::optional<json_query::Token> parseCompare (QString, QVector<json_query::FilterFn>&);
    std::optional<json_query::Token> parseRegex   (QString, QVector<json_query::FilterFn>&);
}

namespace detail {
    QJsonArray fanOut(const JSONPath&,
                      const json_query::Token&,
                      const QJsonArray&);
}

namespace detail {
    struct KeyBuilder;   // new

    std::optional<json_query::Token> callCompileFilter(const QString&, QVector<json_query::FilterFn>&);

    std::expected<qsizetype, json_query::Error>
    parseBracket(qsizetype, QStringView,
                 KeyBuilder&, QVector<json_query::Token>&,
                 QVector<json_query::FilterFn>&);
}

// ======================================================================
//  JSONPath  – public façade; now created through a factory
// ======================================================================
class JSONPath
{
public:
    enum class Option { None = 0, AsPathList = 1 };

    // -----------------------------------------------------------------
    //  Factory (replaces throwing constructor)                    ★
    // -----------------------------------------------------------------
    using Result = std::expected<JSONPath, json_query::Error>;   // ★
    static Result create(QStringView path,                       // ★
                         Option opt = Option::None);             // ★

    // -----------------------------------------------------------------
    //  Evaluation API (unchanged)
    // -----------------------------------------------------------------
    [[nodiscard]] QJsonValue evaluate    (const QJsonDocument& doc  ) const;
    [[nodiscard]] QJsonValue evaluate    (const QJsonValue&    value) const;
    [[nodiscard]] QJsonArray evaluateAll (const QJsonDocument& doc  ) const;
    [[nodiscard]] QJsonArray evaluateAll (const QJsonValue&    value) const;

    [[nodiscard]] bool    isValid()  const { return m_valid; }
    [[nodiscard]] QString toString() const { return m_originalPath; }

    //  Move / copy remain defaulted
    JSONPath(JSONPath&&)            noexcept = default;
    JSONPath(const JSONPath&)                = default;
    JSONPath& operator=(JSONPath&&) noexcept = default;
    JSONPath& operator=(const JSONPath&)     = default;

    //  Aliases exported for callers
    using FunctionType = json_path::FunctionType;
    using Slice        = json_query::Slice;
    using Token        = json_query::Token;
    using Error        = json_query::Error;
    using FilterFn     = json_query::FilterFn;

private:
    // -----------------------------------------------------------------
    //  Private “data” ctor – used only by factory                     ★
    // -----------------------------------------------------------------
    JSONPath( Option                      opt,
              FunctionType                func,
              QString                     original,
              QVector<Token>              tokens,
              QVector<json_query::FilterFn> filters ) noexcept
        : m_valid(true)
        , m_func(func)
        , m_option(opt)
        , m_originalPath(std::move(original))
        , m_tokens(std::move(tokens))
        , m_filters(std::move(filters))
    {}

    // -----------------------------------------------------------------
    //  Internal helpers
    // -----------------------------------------------------------------
    static QJsonValue evalAsPathList(const JSONPath&, const QJsonValue&);
    static QJsonValue evalStandard  (const JSONPath&, const QJsonValue&);

    // -----------------------------------------------------------------
    //  Parsing helpers (exception-free, use std::expected)         ★
    // -----------------------------------------------------------------
    struct Compiled { QVector<Token> tokens; QVector<json_query::FilterFn> filters; };  // ★
    static std::expected<Compiled, Error> compilePath(QStringView sv);                  // ★
    static FunctionType detectTrailingFunction(QString&);                               // ★
    static std::optional<Token> compileFilter(const QString& expr, QVector<json_query::FilterFn>& out);

    [[nodiscard]] QJsonArray evaluateToken     (const Token& tk, const QJsonValue& v) const;
    [[nodiscard]] QJsonArray evaluateRecursive (const QJsonValue& value, int)  const;
    [[nodiscard]] QJsonArray evalSlice         (const QJsonArray& arr,
                                  const Slice& s) const;
    [[nodiscard]] int        normalizeIndex    (int idx, int size) const;
    [[nodiscard]] QJsonArray wildcardObject    (const QJsonObject& obj) const;
    [[nodiscard]] QJsonArray wildcardArray     (const QJsonArray&  arr) const;
    [[nodiscard]] QJsonArray recursiveDescend  (const QJsonValue& v,
                                  const QString& prop) const;
    [[nodiscard]] QString     segmentToPointer (const QString& seg) const;
    [[nodiscard]] static QString escapePointerSegment(const QString&);

    // -----------------------------------------------------------------
    //  Data members
    // -----------------------------------------------------------------
    bool                       m_valid   {true};
    FunctionType               m_func    {FunctionType::None};
    Option                     m_option  {Option::None};
    QString                    m_originalPath;
    QVector<Token>             m_tokens;
    QVector<json_query::FilterFn> m_filters;

    template<Token::Kind K>
    friend QJsonArray detail::eval(const JSONPath&,
                const Token&,
                const QJsonValue&);

    // Give the rule parsers access to the private static compileFilter()
    friend std::optional<Token> detail::parseOr     (QString, QVector<FilterFn>&);
    friend std::optional<Token> detail::parseAnd    (QString, QVector<FilterFn>&);
    friend std::optional<Token> detail::parseIn     (QString, QVector<FilterFn>&);
    friend std::optional<Token> detail::parseCompare(QString, QVector<FilterFn>&);
    friend std::optional<Token> detail::parseRegex  (QString, QVector<FilterFn>&);
    // Grant access to free compile() wrapper
    friend std::expected<JSONPath::Compiled, json_query::Error> compile(QStringView);

    friend QJsonArray detail::fanOut(const JSONPath&,
                                 const Token&,
                                 const QJsonArray&);

    /* existing friends … */
    friend std::expected<qsizetype, json_query::Error>
           detail::parseBracket(qsizetype, QStringView,
                                detail::KeyBuilder&, QVector<json_query::Token>&,
                                QVector<json_query::FilterFn>&);

    friend std::optional<Token> detail::callCompileFilter(const QString&, QVector<FilterFn>&);
};

// ---------------------------------------------------------------------------
//  Free wrapper: json_query::compile(path)
//  Provides Jayway-style compile API returning token+filter structure.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::expected<JSONPath::Compiled, json_query::Error>
compile(QStringView path)
{
    return JSONPath::compilePath(path);
}

namespace json_query {
    using ::compile;
}


