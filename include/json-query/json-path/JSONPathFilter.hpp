// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

// Consolidated header for the JSONPath filter subsystem.
// Merges declarations previously spread across:
//   JSONPathFilter.hpp, JSONPathFilterHelpers.hpp,
//   JSONPathFilterFunctions.hpp, JSONPathFilterOrchestration.hpp

// ────────────────────────────── Qt
#include <QString>
#include <QJsonValue>

// ────────────────────────────── STL
#include <array>
#include <functional>
#include <optional>
#include <vector>

// ────────────────────────────── Project
#include "json-query/json-path/JSONPathCompile.hpp"

// ======================================================================
//  Public API – filter compilation entry point
// ======================================================================
#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_path
{

/**
 * @brief Compile a JSONPath filter expression into executable filter functions
 *
 * @param expr The filter expression string to compile
 * @param out Vector to store the compiled filter functions
 * @return Optional Token representing the compiled filter, or nullopt if compilation failed
 */
std::optional<Token> compileFilter(const QString& expr, std::vector<FilterFn>& out);

} // namespace json_query::json_path

// ======================================================================
//  Internal helpers, functions, and orchestration
// ======================================================================
namespace json_query::inline JSON_QUERY_ABI_NS::json_path::detail
{

using Token      = json_path::Token;
using FilterFn   = json_path::FilterFn;
using ParseError = json_path::ParseError;

// ---------------------------------------------------------------------
//  Helper utilities  (was JSONPathFilterHelpers.hpp)
// ---------------------------------------------------------------------

[[nodiscard]] bool isValidJsonNumber(const QString& value) noexcept;
bool               unquote(QString& s);
QString            stripOuterParens(QString s);

// A tiny facade so every parser can push a predicate and obtain the corresponding Token
class Builder
{
  public:
    std::vector<json_query::json_path::FilterFn>& fns;

    [[nodiscard]] json_query::json_path::Token add(json_query::json_path::FilterFn fn, QString key = {});
};

// ---------------------------------------------------------------------
//  Function evaluation  (was JSONPathFilterFunctions.hpp)
// ---------------------------------------------------------------------

QJsonValue evaluateFunction(const QString& funcExpr, const QJsonValue& context);
QJsonValue parseJsonLiteral(const QString& literal);
int        compareValues(const QJsonValue& left, const QJsonValue& right);

// ---------------------------------------------------------------------
//  Parser rule declarations  (was JSONPathFilter.hpp + Orchestration)
// ---------------------------------------------------------------------

[[nodiscard]] std::optional<Token> parseOr(const QString& s, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseAnd(const QString& s, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseNot(const QString& s, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseAbsolutePath(const QString& s, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseIn(const QString& s, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseExists(const QString& s, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseSelfCmp(const QString& s, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseFunction(const QString& s, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseCompare(const QString& s, std::vector<FilterFn>& out);
[[nodiscard]] std::optional<Token> parseRegex(const QString& s, std::vector<FilterFn>& out);

// Table-driven parser dispatch system
using RuleFn = std::optional<json_query::json_path::Token> (*)(const QString&,
                                                               std::vector<json_query::json_path::FilterFn>&);

/**
 * @brief Rules ordered by precedence (lowest precedence first).
 * The dispatcher tries each rule in order until one succeeds.
 */
extern const std::array<RuleFn, 10> rules;

} // namespace json_query::json_path::detail
