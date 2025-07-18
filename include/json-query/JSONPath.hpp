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
#include "json-path/JSONPathCompiler.hpp"
#include "JSONQueryUtils.hpp"
#include "JSONPointer.hpp"

#include <ctre.hpp>

using namespace Qt::StringLiterals;

namespace json_query
{

using FilterFn = std::function<bool (const QJsonValue&)>;

} // namespace json_query

class JSONPath;

namespace detail
{
    template<json_query::json_path::Token::Kind K>
    QJsonArray eval(const JSONPath&,
                    const json_query::json_path::Token&,
                    const QJsonValue&);
}

namespace detail {

    using json_query::json_path::Token;
    using json_query::FilterFn;
    using json_query::json_path::Error;

    std::optional<Token> parseOr      (QString, QVector<FilterFn>&);
    std::optional<Token> parseAnd     (QString, QVector<FilterFn>&);
    std::optional<Token> parseIn      (QString, QVector<FilterFn>&);
    std::optional<Token> parseCompare (QString, QVector<FilterFn>&);
    std::optional<Token> parseRegex   (QString, QVector<FilterFn>&);

    QJsonArray fanOut(const JSONPath&,
                      const Token&,
                      const QJsonArray&);

    struct KeyBuilder;   // new

    std::optional<Token> callCompileFilter(const QString&, QVector<FilterFn>&);

    std::expected<qsizetype, Error>
    parseBracket(qsizetype, QStringView,
                 KeyBuilder&, QVector<Token>&,
                 QVector<FilterFn>&);
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
    using Result = std::expected<JSONPath, json_query::json_path::Error>;   // ★
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
    using FunctionType = json_query::json_path::FunctionType;
    using Slice        = json_query::json_path::Slice;
    using Token        = json_query::json_path::Token;
    using Error        = json_query::json_path::Error;
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
    QVector<FilterFn> m_filters;

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
    friend std::expected<JSONPath::Compiled, Error> compile(QStringView);

    friend QJsonArray detail::fanOut(const JSONPath&,
                                 const Token&,
                                 const QJsonArray&);

    /* existing friends … */
    friend std::expected<qsizetype, Error>
           detail::parseBracket(qsizetype, QStringView,
                                detail::KeyBuilder&, QVector<Token>&,
                                QVector<FilterFn>&);

    friend std::optional<Token> detail::callCompileFilter(const QString&, QVector<FilterFn>&);
};

// ---------------------------------------------------------------------------
//  Free wrapper: json_query::compile(path)
//  Provides Jayway-style compile API returning token+filter structure.
// ---------------------------------------------------------------------------
[[nodiscard]] inline std::expected<JSONPath::Compiled, json_query::json_path::Error>
compile(QStringView path)
{
    return JSONPath::compilePath(path);
}

namespace json_query {
    using ::compile;
}


