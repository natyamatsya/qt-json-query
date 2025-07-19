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
#include "JSONPathCompile.hpp"
#include "../JSONQueryUtils.hpp"
#include "../JSONPointer.hpp"
#include "json-query/json-path/JSONPathOption.hpp"

#include <ctre.hpp>

using namespace Qt::StringLiterals;

// Forward declaration for the detail namespace
namespace json_query {
    class JSONPath;
}

namespace json_query::json_path::detail {

    class PathEvalCtx; // forward declaration for friend

    // forward declarations of new evaluator free-functions
    QJsonValue evaluate(const PathEvalCtx&, const json_query::JSONPath&, const QJsonValue&);
    QJsonArray evaluateAll(const PathEvalCtx&, const json_query::JSONPath&, const QJsonValue&);

    // (internal filter-parsing helpers moved to JSONPathFilter.cpp – no longer
    //  forward-declared here)

} // namespace json_query::json_path::detail

// ======================================================================
//  JSONPath  – public façade; now uses JSONPathCompiler internally
// ======================================================================
namespace json_query {

class JSONPath
{
public:
    using Option = json_query::json_path::Option;

    // -----------------------------------------------------------------
    //  Factory (replaces throwing constructor)                    ★
    // -----------------------------------------------------------------
    using Result = std::expected<JSONPath, json_path::Error>;
    // ★
    static Result create(QStringView path,                       // ★
                             Option opt = Option::None);
    // ★
    // -----------------------------------------------------------------
    //  Evaluation API (unchanged)
    // -----------------------------------------------------------------
    [[nodiscard]] QJsonValue evaluate    (const QJsonDocument& doc  ) const;
    [[nodiscard]] QJsonValue evaluate    (const QJsonValue&    value) const;
    [[nodiscard]] QJsonArray evaluateAll (const QJsonDocument& doc  ) const;
    [[nodiscard]] QJsonArray evaluateAll (const QJsonValue&    value) const;

    [[nodiscard]] QString toString() const { return m_originalPath; }

    //  Move / copy remain defaulted
    JSONPath(JSONPath&&)            noexcept = default;
    JSONPath(const JSONPath&)                = default;
    JSONPath& operator=(JSONPath&&) noexcept = default;
    JSONPath& operator=(const JSONPath&)     = default;

    //  Aliases exported for callers
    using FunctionType = json_path::FunctionType;
    using Slice        = json_path::Slice;
    using Token        = json_path::Token;
    using FilterFn     = json_path::FilterFn;

    // (no friend declarations needed for pure evaluators)

private:
    // -----------------------------------------------------------------
    //  Private "data" ctor – used only by factory                     ★
    // -----------------------------------------------------------------
    JSONPath( Option                      opt,
                  FunctionType                func,
                  QString                     original,
                  QVector<Token>              tokens,
                  QVector<FilterFn> filters ) noexcept
            : m_func(func)
            , m_option(opt)
            , m_originalPath(std::move(original))
            , m_tokens(std::move(tokens))
            , m_filters(std::move(filters))
        {}

    // -----------------------------------------------------------------
    //  Internal helpers
    // -----------------------------------------------------------------
    // Top-level evaluation helpers have been migrated to PathEvaluator.cpp.

    [[nodiscard]] QJsonArray evaluateToken     (const Token& tk, const QJsonValue& v) const;
    [[nodiscard]] QJsonArray evaluateRecursive (const QJsonValue& value, int)  const;
    [[nodiscard]] QJsonArray evalSlice         (const QJsonArray& arr,
                                      const Slice& s) const;
    [[nodiscard]] int        normalizeIndex    (int idx, int size) const;
    [[nodiscard]] QJsonArray wildcardObject    (const QJsonObject& obj) const;
    [[nodiscard]] QJsonArray wildcardArray     (const QJsonArray&  arr) const;

    // -----------------------------------------------------------------
    //  Data members
    // -----------------------------------------------------------------
    FunctionType               m_func    {FunctionType::None};
    Option                     m_option  {Option::None};
    QString                    m_originalPath;
    QVector<Token>             m_tokens;
    QVector<FilterFn> m_filters;

}; // end class JSONPath

} // namespace json_query
