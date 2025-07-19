#pragma once

#include <QJsonValue>
#include <QJsonArray>

#include "json-query/json-path/PathEvalCtx.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"

namespace json_query {
    class JSONPath; // fwd
}

namespace json_query::json_path::detail {

// Basic helpers (exposed for tests)
int        normalizeIndex(int idx, int size);
QJsonArray evalSlice      (const QJsonArray& arr, const Slice& s);

// Core evaluation API (phase-B transitional): still requires the JSONPath
// facade for token dispatch but takes an explicit context for data.
QJsonValue evaluate     (const PathEvalCtx& ctx, const json_query::JSONPath& self, const QJsonValue& root);
QJsonArray evaluateAll  (const PathEvalCtx& ctx, const json_query::JSONPath& self, const QJsonValue& root);

// ---------------------------------------------------------------------
// Phase C – fully decoupled evaluation helpers (no JSONPath dependency)
// ---------------------------------------------------------------------

// Token dispatcher (migrated former JSONPath::evaluateToken)
QJsonArray evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);

// Fan-out one token over an array of input values
QJsonArray fanOut       (const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src);

// Complete evaluation pipelines (migrated former evalStandard / evalAsPathList)
QJsonValue evalStandard   (const PathEvalCtx& ctx, const QJsonValue& root);
QJsonValue evalAsPathList (const PathEvalCtx& ctx, const QJsonValue& root);

// Convenience top-level entry that selects strategy based on ctx.option
QJsonValue evaluate       (const PathEvalCtx& ctx, const QJsonValue& root);

QJsonArray evaluateAll    (const PathEvalCtx& ctx, const QJsonValue& root);

} // namespace json_query::json_path::detail
