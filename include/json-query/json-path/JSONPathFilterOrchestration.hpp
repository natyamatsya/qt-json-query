#pragma once

#include <QString>
#include <vector>
#include <optional>
#include <array>

#include "JSONPathCompile.hpp"

namespace json_query::json_path
{

using json_query::json_path::FilterFn;
using json_query::json_path::Token;

/**
 * @brief Main filter compilation orchestration
 *
 * This module provides the primary entry point for compiling JSONPath filter expressions
 * into executable filter functions. It uses a table-driven dispatch system with precedence
 * ordering to route expressions to appropriate specialized parsers.
 */

/**
 * @brief Compile a JSONPath filter expression into executable filter functions
 *
 * @param expr The filter expression string to compile
 * @param out Vector to store the compiled filter functions
 * @return Optional Token representing the compiled filter, or nullopt if compilation failed
 */
std::optional<Token> compileFilter(const QString& expr, std::vector<FilterFn>& out);

} // namespace json_query::json_path

namespace json_query::json_path::detail
{

// Rule function type for parser dispatch
using RuleFn = std::optional<json_query::json_path::Token> (*)(const QString&,
                                                               std::vector<json_query::json_path::FilterFn>&);

// Parser function declarations (implemented in various specialized modules)
std::optional<json_query::json_path::Token> parseOr(const QString&                                s,
                                                    std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseAnd(const QString&                                s,
                                                     std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseNot(const QString&                                s,
                                                     std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseAbsolutePath(const QString&                                s,
                                                              std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseIn(const QString&                                s,
                                                    std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseExists(const QString&                                s,
                                                        std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseSelfCmp(const QString&                                s,
                                                         std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseFunction(const QString&                                s,
                                                          std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseCompare(const QString&                                s,
                                                         std::vector<json_query::json_path::FilterFn>& out);
std::optional<json_query::json_path::Token> parseRegex(const QString&                                s,
                                                       std::vector<json_query::json_path::FilterFn>& out);

/**
 * @brief Table-driven parser dispatch system
 *
 * Rules are ordered by precedence (lowest precedence first).
 * The dispatcher tries each rule in order until one succeeds.
 */
extern const std::array<RuleFn, 10> rules;

} // namespace json_query::json_path::detail
