#include "json-query/json-path/JSONPathTokenDispatch.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathWildcardRecursive.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"
#include "json-query/json-path/internal/CompileTimeTokenDispatch.hpp"
#include "json-query/json-path/internal/TokenDispatchTable.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

#include <QDebug>

namespace json_query::json_path::detail {

using json_query::json_path::internal::TokenDispatcher;
using internal::acquirePooledArray;

// ---------------------------------------------------------------------------
//  Token evaluation dispatch system
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    qCDebug(jsonPathLog) << "[stage] token" << (&tk - ctx.tokens.data()) << ": kind=" << static_cast<int>(tk.kind) << "working size=" << 1;

    // Use TableGen-inspired dispatch table
    return TokenDispatcher::dispatch(ctx, tk, v);
}

} // namespace json_query::json_path::detail

// ---------------------------------------------------------------------------
//  TableGen-inspired token dispatch function implementations
// ---------------------------------------------------------------------------

namespace json_query::json_path::internal {

using namespace json_query::json_path::detail;

// Simple dispatch functions that call the actual eval functions
std::expected<QJsonArray, EvalError> dispatchKey(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::Key>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchIndex(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::Index>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchSlice(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::Slice>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchWildcard(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::Wildcard>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchRecursive(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::Recursive>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchFilter(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::Filter>(ctx, tk, v);
}

std::expected<QJsonArray, EvalError> dispatchKeyList(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
{
    return json_query::json_path::detail::eval<Token::Kind::KeyList>(ctx, tk, v);
}

} // namespace json_query::json_path::internal

// ---------------------------------------------------------------------------
//  TableGen-inspired Error Handling Dispatch Implementation
// ---------------------------------------------------------------------------

namespace json_query::json_path::internal {

// Template specialization implementation for recursive dispatch
template<ErrorHandlingStrategy FirstStrategy, ErrorHandlingStrategy... RestStrategies>
template<internal::ResultStreamerConcept StreamerType>
bool ErrorHandlingDispatchTable<FirstStrategy, RestStrategies...>::dispatch(
    const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx,
    const QJsonArray& src, const StreamerType& streamer) {
    
    if constexpr (ErrorHandlingDef<FirstStrategy>::enabled) {
        if (ErrorHandlingDef<FirstStrategy>::matches(tk, tokenPos, ctx)) {
            return processWithStrategy<FirstStrategy>(tk, tokenPos, ctx, src, streamer);
        }
    }
    
    // Try next strategy in the dispatch table
    return ErrorHandlingDispatchTable<RestStrategies...>::dispatch(tk, tokenPos, ctx, src, streamer);
}

// Private helper for strategy processing
template<ErrorHandlingStrategy FirstStrategy, ErrorHandlingStrategy... RestStrategies>
template<ErrorHandlingStrategy Strategy, internal::ResultStreamerConcept StreamerType>
bool ErrorHandlingDispatchTable<FirstStrategy, RestStrategies...>::processWithStrategy(
    const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx,
    const QJsonArray& src, const StreamerType& streamer) {
    
    bool anySuccess = false;
    EvalError lastError = EvalError::TypeMismatchObject; // Default error
    bool shouldEarlyReturn = false;
    
    for (const auto& srcValue : src) {
        // Use compile-time dispatch to eliminate runtime switch overhead
        std::expected<QJsonArray, EvalError> result = 
            internal::CompileTimeTokenDispatcher::dispatch(ctx, tk, srcValue);
        
        if (result) {
            // Success: stream results directly
            anySuccess = true;
            streamer.emitArray(*result);
        } else {
            // Failure: process according to strategy
            ErrorHandlingProcessor<Strategy>::processFailure(tk, result.error(), streamer, anySuccess, lastError);
            
            // Check for early return (strict error handling)
            if constexpr (Strategy == ErrorHandlingStrategy::StrictPropertyChain) {
                shouldEarlyReturn = true;
                break;
            }
        }
    }
    
    if (shouldEarlyReturn) {
        return true; // Indicate early return occurred
    }
    
    // Process final result according to strategy
    ErrorHandlingProcessor<Strategy>::processFinalResult(tk, streamer, anySuccess, lastError);
    return false; // Normal completion
}

} // namespace json_query::json_path::internal

// ---------------------------------------------------------------------------
//  Explicit Template Instantiations
// ---------------------------------------------------------------------------

namespace json_query::json_path::internal {

// Explicit instantiation for the specific template parameters used in the codebase
template bool ErrorHandlingDispatchTable<
    ErrorHandlingStrategy::PermissiveDirectAccess,
    ErrorHandlingStrategy::PermissiveRecursiveContext,
    ErrorHandlingStrategy::StrictPropertyChain,
    ErrorHandlingStrategy::DefaultPermissive
>::dispatch<internal::ResultStreamer<internal::ResultCollector>>(
    const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx,
    const QJsonArray& src, const internal::ResultStreamer<internal::ResultCollector>& streamer);

} // namespace json_query::json_path::internal
