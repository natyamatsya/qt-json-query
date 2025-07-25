#include "json-query/json-path/JSONPathTokenDispatch.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathWildcardRecursive.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"
#include "json-query/json-path/internal/TokenDispatchTable.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

#include <QDebug>

namespace json_query::json_path::detail {

using json_query::json_path::internal::ResultStreamer;
using json_query::json_path::internal::TokenDispatcher;
using internal::acquirePooledArray;

// ---------------------------------------------------------------------------
//  Token evaluation dispatch system
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> evaluateTokenExpected(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    qCDebug(jsonPathLog) << "[stage] token" << (&tk - ctx.tokens.data()) << ": kind=" << static_cast<int>(tk.kind) << "working size=" << 1;

    // Use TableGen-inspired dispatch table
    return TokenDispatcher::dispatch(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return evaluateTokenExpected(ctx, tk, v);
}

// ---------------------------------------------------------------------------
//  Streaming-optimized fan-out helper
// ---------------------------------------------------------------------------

void fanOutStreaming(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, 
                    const ResultStreamer& streamer, qsizetype tokenPos)
{
    qCDebug(jsonPathLog) << "[fanOutStreaming] kind=" << static_cast<int>(tk.kind) 
                         << "srcType" << static_cast<int>(src.first().type()) 
                         << "seg size=" << src.size();

    for (const auto& srcValue : src) {
        auto result = evaluateTokenExpected(ctx, tk, srcValue);
        if (result) {
            // Use emitArray to stream all results directly
            streamer.emitArray(*result);
        } else {
            qCDebug(jsonPathLog) << "[fanOutStreaming] token evaluation failed:" << static_cast<int>(result.error());
            // Continue with other values even if one fails
        }
    }
}

} // namespace json_query::json_path::detail

// ---------------------------------------------------------------------------
//  TableGen-inspired token dispatch function implementations
// ---------------------------------------------------------------------------

namespace json_query::json_path::internal {

using namespace json_query::json_path::detail;

// Simple dispatch functions that call the actual evalExpected functions
std::expected<QJsonArray, EvalError> dispatchKey(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::evalExpected<Token::Kind::Key>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchIndex(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::evalExpected<Token::Kind::Index>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchSlice(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::evalExpected<Token::Kind::Slice>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchWildcard(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::evalExpected<Token::Kind::Wildcard>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchRecursive(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::evalExpected<Token::Kind::Recursive>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchFilter(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::evalExpected<Token::Kind::Filter>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchKeyList(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::evalExpected<Token::Kind::KeyList>(ctx, tk, v);
}

} // namespace json_query::json_path::internal
