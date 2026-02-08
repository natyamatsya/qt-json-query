// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-path/JSONPathError.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/JSONPathEvalHelpers.hpp"

#include <QJsonValue>
#include <QJsonArray>
#include <expected>

// (legacy JSONPath bridge removed)

namespace json_query::json_path::detail
{

// Internal error carrying both the error code and the failing token index
struct DetailedEvalError
{
    EvalError     error{};
    std::uint16_t tokenIndex{};
};

// Raw evaluation result: the RFC 9535 node list + whether the path selects multiple nodes
struct NodeList
{
    QJsonArray nodes{};
    bool       multi{false};
};

// ---------------------------------------------------------------------------
//  Core evaluation API with std::expected error handling
// ---------------------------------------------------------------------------

// Walk the token stream and return the raw node list (no squash, no trailing fn)
std::expected<NodeList, DetailedEvalError> evaluateTokenStream(const PathEvalCtx& ctx, const QJsonValue& root);

// Convenience wrappers that squash / apply trailing function
std::expected<QJsonArray, DetailedEvalError> evaluate(const PathEvalCtx& ctx, const QJsonValue& root);
std::expected<QJsonValue, DetailedEvalError> evaluateSingle(const PathEvalCtx& ctx, const QJsonValue& root);

// Fast path for definite JSONPaths (only Key/Index selectors, no wildcard/recursive/filter)
std::expected<NodeList, DetailedEvalError> evaluateDefinite(const std::vector<Token>& tokens,
                                                            const QJsonValue&         root) noexcept;

// Apply a single token to every element in src, collecting results
std::expected<QJsonArray, EvalError>
fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos);

} // namespace json_query::json_path::detail

// Include inline implementations for critical hot path functions
#include "internal/JSONPathEvaluate.inl"
