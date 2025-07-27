#pragma once

#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/PassPipeline.hpp"
#include "json-query/json-path/JSONPathEvalHelpers.hpp"
#include "json-query/json-path/JSONPathTokenDispatch.hpp"
#include "json-query/json-path/JSONPathWildcardRecursive.hpp"

#include <QJsonValue>
#include <QJsonArray>
#include <expected>

#include "json-query/json-path/JSONPathCompile.hpp"

// (legacy JSONPath bridge removed)

namespace json_query::json_path::detail {

// ---------------------------------------------------------------------------
//  Core evaluation API with std::expected error handling
// ---------------------------------------------------------------------------

// Complete evaluation pipelines with std::expected error handling
std::expected<QJsonValue, EvalError> evalStandard(const PathEvalCtx& ctx, const QJsonValue& root);
std::expected<QJsonArray, EvalError> evaluateAll(const PathEvalCtx& ctx, const QJsonValue& root);

// Convenience top-level entry that uses std::expected
std::expected<QJsonValue, EvalError> evaluate(const PathEvalCtx& ctx, const QJsonValue& root);

// Direct array-based fan-out using TableGen dispatch - critical hot path
std::expected<QJsonArray, EvalError> fanOut(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, qsizetype tokenPos);

} // namespace json_query::json_path::detail

// Include inline implementations for critical hot path functions
#include "internal/JSONPathEvaluate.inl"
