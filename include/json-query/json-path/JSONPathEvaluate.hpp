#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>

#include "internal/PathEvalCtx.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"

// (legacy JSONPath bridge removed)

namespace json_query::json_path::detail {

// Basic helpers (exposed for tests)
int        normalizeIndex(int idx, int size);
QJsonArray evalSlice      (const QJsonArray& arr, const Slice& s);

// Core evaluation API (fully decoupled, pure helpers)
QJsonArray evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);

// Fan-out one token over an array of input values
QJsonArray fanOut       (const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src);

// Complete evaluation pipelines (migrated former evalStandard)
QJsonValue evalStandard   (const PathEvalCtx& ctx, const QJsonValue& root);
QJsonArray evaluateAll    (const PathEvalCtx& ctx, const QJsonValue& root);

// Convenience top-level entry that selects strategy based on ctx.option
QJsonValue evaluate       (const PathEvalCtx& ctx, const QJsonValue& root);

// Wildcard and recursive helpers
QJsonArray wildcardObject(const QJsonObject& obj);
QJsonArray wildcardArray (const QJsonArray& arr);
QJsonArray evaluateRecursive(const QJsonValue& value, int unused = 0);

} // namespace json_query::json_path::detail
