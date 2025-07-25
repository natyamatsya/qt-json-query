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
#include "json-query/utils/JSONQueryUtils.hpp"
#include "json-query/json-pointer/JSONPointer.hpp"
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/PassPipeline.hpp"

using namespace Qt::StringLiterals;
// ======================================================================
//  JSONPath
// ======================================================================
namespace json_query {

class JSONPath
{
public:
    // -----------------------------------------------------------------
    //  Factory (replaces throwing constructor)                    ★
    // -----------------------------------------------------------------
    using Result = std::expected<JSONPath, json_query::json_path::Error>;
    // ★
    static Result create(QStringView path);                      // ★
    // LLVM-inspired compilation with optimization levels
    static Result create(QStringView path, json_query::json_path::internal::PassManager::OptimizationLevel optLevel);
    // ★
    // -----------------------------------------------------------------
    //  Evaluation API with error reporting (std::expected)
    // -----------------------------------------------------------------
    using EvalResult = std::expected<QJsonValue, json_query::json_path::EvalError>;
    using EvalArrayResult = std::expected<QJsonArray, json_query::json_path::EvalError>;

    [[nodiscard]] EvalResult evaluate(const QJsonDocument& doc) const;
    [[nodiscard]] EvalResult evaluate(const QJsonValue&    value) const;
    
    [[nodiscard]] EvalArrayResult evaluateAll(const QJsonDocument& doc) const;
    [[nodiscard]] EvalArrayResult evaluateAll(const QJsonValue&    value) const;

    // -----------------------------------------------------------------
    //  Other
    // -----------------------------------------------------------------
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
    using FilterFn     = json_query::json_path::FilterFn;
    using ContextFilterFn = json_query::json_path::ContextFilterFn;
    using Error = json_query::json_path::Error;

private:
    // -----------------------------------------------------------------
    //  Private "data" ctor – used only by factory                     ★
    // -----------------------------------------------------------------
    JSONPath( json_query::json_path::FunctionType                func,
                  QString                     original,
                  QVector<json_query::json_path::Token>              tokens,
                  QVector<json_query::json_path::FilterFn> filters,
                  QVector<json_query::json_path::ContextFilterFn> contextFilters ) noexcept
            : m_func(func)
            , m_originalPath(std::move(original))
            , m_tokens(std::move(tokens))
            , m_filters(std::move(filters))
            , m_contextFilters(std::move(contextFilters))
        {}

    // -----------------------------------------------------------------
    //  Data members
    // -----------------------------------------------------------------
    json_query::json_path::FunctionType               m_func    {json_query::json_path::FunctionType::None};
    QString                    m_originalPath;
    QVector<json_query::json_path::Token>             m_tokens;
    QVector<json_query::json_path::FilterFn> m_filters;
    QVector<json_query::json_path::ContextFilterFn> m_contextFilters;

}; // end class JSONPath

} // namespace json_query
