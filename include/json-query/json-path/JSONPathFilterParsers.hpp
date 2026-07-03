// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#pragma once

#include <QString>
#include <QVector>
#include <QJsonValue>
#include "json-query/utils/BraceSafe.hpp"

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <optional>
#include <functional>
#include <vector>
#include "json-query/json-path/JSONPathFilterComparison.hpp"
#include "json-query/json-path/JSONPathFilter.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include <ctre.hpp>

namespace json_query::json_path
{
using FilterFn = std::function<bool(const QJsonValue&)>;
struct Token;
} // namespace json_query::json_path

namespace json_query::json_path::detail
{

using json_query::utils::to_qt_s;
using json_query::utils::to_sv;

// Forward declarations
class FilterParseContext;

// ──────────────────────────────────────────────────────────────────────
//  Template Function Declarations
// ──────────────────────────────────────────────────────────────────────

// Self-value comparison templates
template <auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token>
parseSelfValue(QString s, std::vector<json_query::json_path::FilterFn>& out);

// Property comparison templates
template <auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token>
parseCompare1(QString s, std::vector<json_query::json_path::FilterFn>& out);

template <auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token>
parseCompareIndex(QString s, std::vector<json_query::json_path::FilterFn>& out);

// Regex pattern templates
template <auto PAT>
std::optional<Token> parseRegex1(QString s, std::vector<FilterFn>& out);

// Self-comparison templates
template <auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token>
parseSelfCompare(QString s, std::vector<json_query::json_path::FilterFn>& out);

template <auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token>
parseSelfCompareIndex(QString s, std::vector<json_query::json_path::FilterFn>& out);

// Null comparison templates
template <auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token>
parseNullCompare(QString s, std::vector<json_query::json_path::FilterFn>& out);

template <auto PAT>
[[nodiscard]] std::optional<json_query::json_path::Token>
parseNullCompareIndex(QString s, std::vector<json_query::json_path::FilterFn>& out);

// Embedded filter parser templates
template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedCompare1(const QString& s);

template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedCompareIndex(const QString& s);

template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedSelfValue(const QString& s);

template <ctll::fixed_string Pattern>
std::optional<Token> parseEmbeddedRegex1(const QString& s);

// ──────────────────────────────────────────────────────────────────────
//  Non-Template Function Declarations
// ──────────────────────────────────────────────────────────────────────

// Individual parser functions
std::optional<json_query::json_path::Token> parseOr(const QString&                                s,
                                                    std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseAnd(const QString&                                s,
                                                     std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseIn(const QString&                                s,
                                                    std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseCompare(const QString&                                s,
                                                         std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseRegex(const QString&                                s,
                                                       std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseExists(const QString&                                s,
                                                        std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseSelfCmp(const QString&                                s,
                                                         std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseNot(const QString&                                s,
                                                     std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseAbsolutePath(const QString&                                s,
                                                              std::vector<json_query::json_path::FilterFn>& out);

// Forward declarations
std::optional<Token> parseOr(const QString& s, std::vector<FilterFn>& out);
std::optional<Token> parseAnd(const QString& s, std::vector<FilterFn>& out);
std::optional<Token> parseIn(const QString& s, std::vector<FilterFn>& out);

// JSON literal parsing utility
QJsonValue parseJsonLiteral(const QString& value);

// Other parser functions

// ──────────────────────────────────────────────────────────────────────
//  Modern Embedded Filter Parser Functions (Zero-Overhead)
// ──────────────────────────────────────────────────────────────────────

// Individual embedded filter parser functions
std::optional<Token> parseEmbeddedOr(const QString& s);
std::optional<Token> parseEmbeddedAnd(const QString& s);
std::optional<Token> parseEmbeddedIn(const QString& s);
std::optional<Token> parseEmbeddedCompare(const QString& s);
std::optional<Token> parseEmbeddedRegex(const QString& s);
std::optional<Token> parseEmbeddedExists(const QString& s);
std::optional<Token> parseEmbeddedSelfCmp(const QString& s);
std::optional<Token> parseEmbeddedNot(const QString& s);

#include "json-query/json-path/JSONPathFilterParsers.inl"

} // namespace json_query::json_path::detail
