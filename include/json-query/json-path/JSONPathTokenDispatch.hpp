#pragma once

#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"

#include <QJsonValue>
#include <QJsonArray>
#include <expected>

namespace json_query::json_path::detail {

using json_query::json_path::internal::ResultStreamer;

// ---------------------------------------------------------------------------
//  Token evaluation dispatch system
// ---------------------------------------------------------------------------

// Core token evaluation with std::expected error handling
std::expected<QJsonArray, EvalError> evaluateTokenExpected(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);

// Fan-out functionality for applying tokens to arrays of values
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos = -1);

// Streaming-optimized fan-out (internal)
void fanOutStreaming(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, 
                    const ResultStreamer& streamer, qsizetype tokenPos = -1);

} // namespace json_query::json_path::detail

namespace json_query::json_path::internal {

// ---------------------------------------------------------------------------
//  TableGen-inspired token dispatch functions
// ---------------------------------------------------------------------------

// Individual token type dispatchers
std::expected<QJsonArray, EvalError> dispatchKey(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchIndex(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchSlice(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchWildcard(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchRecursive(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchFilter(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchKeyList(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);

} // namespace json_query::json_path::internal
