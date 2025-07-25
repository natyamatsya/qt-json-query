#pragma once

#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/PassPipeline.hpp"

#include <QJsonValue>
#include <QJsonArray>
#include <expected>

#include "json-query/json-path/JSONPathCompile.hpp"

// (legacy JSONPath bridge removed)

namespace json_query::json_path::detail {

// Basic helpers (exposed for tests)
int        normalizeIndex(int idx, int size);
std::expected<QJsonArray, EvalError> evalSlice(const QJsonArray& arr, const Slice& s);

// Core evaluation API with std::expected error handling
std::expected<QJsonArray, EvalError> evaluateTokenExpected(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);

// Fan-out one token over an array of input values
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src);

// Complete evaluation pipelines with std::expected error handling
std::expected<QJsonValue, EvalError> evalStandard(const PathEvalCtx& ctx, const QJsonValue& root);
std::expected<QJsonArray, EvalError> evaluateAll(const PathEvalCtx& ctx, const QJsonValue& root);

// Convenience top-level entry that uses std::expected
std::expected<QJsonValue, EvalError> evaluate(const PathEvalCtx& ctx, const QJsonValue& root);

// Wildcard and recursive helpers
std::expected<QJsonArray, EvalError> wildcardObject(const QJsonObject& obj);
std::expected<QJsonArray, EvalError> wildcardArray(const QJsonArray& arr);
std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, int unused = 0);

} // namespace json_query::json_path::detail
