#pragma once

// Header exposing the standalone filter-parsing helpers implemented in
// `src/json-query/json-path/JSONPathFilter.cpp`.
// Keeping them in a dedicated header avoids dragging the full JSONPath
// façade for callers that only need filter compilation and allows other
// translation units to forward-declare / use these helpers without
// knowledge of the JSONPath class internals.

// ────────────────────────────── Qt
#include <QString>
#include <QJsonValue>

// ────────────────────────────── STL
#include <optional>
#include <functional>
#include <vector>

// ────────────────────────────── Project
#include "json-query/json-path/JSONPathCompile.hpp" // For json_query::json_path::Token / Error

namespace json_query::json_path::detail
{

using Token    = json_path::Token;
using FilterFn = json_path::FilterFn;

// Same error enum as the main compiler – handy if users need to signal
// syntax errors.
using Error = json_path::Error;

// ---------------------------------------------------------------------
//  Individual rule parsers
// ---------------------------------------------------------------------
[[nodiscard]] std::optional<Token> parseOr(const QString& expr, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseAnd(const QString& expr, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseIn(const QString& expr, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseCompare(const QString& expr, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseRegex(const QString& expr, std::vector<FilterFn>& out);

} // namespace json_query::json_path::detail
