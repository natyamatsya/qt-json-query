#pragma once

// Header exposing the standalone filter-parsing helpers implemented in
// `src/json-query/json-path/JSONPathFilter.cpp`.
// Keeping them in a dedicated header avoids dragging the full JSONPath
// façade for callers that only need filter compilation and allows other
// translation units to forward-declare / use these helpers without
// knowledge of the JSONPath class internals.

// ────────────────────────────── Qt
#include <QString>
#include <QVector>
#include <QJsonValue>

// ────────────────────────────── STL
#include <optional>
#include <functional>

// ────────────────────────────── Project
#include "json-query/json-path/JSONPathCompile.hpp"  // For json_query::json_path::Token / Error

namespace json_query
{
    // Alias kept in a central place so we don’t need to duplicate it.
    using FilterFn = std::function<bool (const QJsonValue&)>;
}

namespace detail
{
    using json_query::json_path::Token;
    using json_query::FilterFn;

    // Same error enum as the main compiler – handy if users need to signal
    // syntax errors.
    using json_query::json_path::Error;

    // ---------------------------------------------------------------------
    //  Individual rule parsers
    // ---------------------------------------------------------------------
    [[nodiscard]] std::optional<Token> parseOr      (QString expr, QVector<FilterFn>& out);
    [[nodiscard]] std::optional<Token> parseAnd     (QString expr, QVector<FilterFn>& out);
    [[nodiscard]] std::optional<Token> parseIn      (QString expr, QVector<FilterFn>& out);
    [[nodiscard]] std::optional<Token> parseCompare (QString expr, QVector<FilterFn>& out);
    [[nodiscard]] std::optional<Token> parseRegex   (QString expr, QVector<FilterFn>& out);

} // namespace detail
