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

// ---------------------------------------------------------------------------
//  TableGen-inspired Error Handling Dispatch Implementation
// ---------------------------------------------------------------------------

namespace json_query::json_path::internal {

// Template specialization implementation for recursive dispatch
template<ErrorHandlingStrategy FirstStrategy, ErrorHandlingStrategy... RestStrategies>
template<typename StreamerType>
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
template<ErrorHandlingStrategy Strategy, typename StreamerType>
bool ErrorHandlingDispatchTable<FirstStrategy, RestStrategies...>::processWithStrategy(
    const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx,
    const QJsonArray& src, const StreamerType& streamer) {
    
    bool anySuccess = false;
    EvalError lastError = EvalError::TypeMismatchObject; // Default error
    bool shouldEarlyReturn = false;
    
    for (const auto& srcValue : src) {
        // Use the appropriate dispatch function based on token kind
        std::expected<QJsonArray, EvalError> result;
        
        switch (tk.kind) {
            case Token::Kind::Key:
                result = dispatchKey(ctx, tk, srcValue);
                break;
            case Token::Kind::Index:
                result = dispatchIndex(ctx, tk, srcValue);
                break;
            case Token::Kind::Slice:
                result = dispatchSlice(ctx, tk, srcValue);
                break;
            case Token::Kind::Wildcard:
                result = dispatchWildcard(ctx, tk, srcValue);
                break;
            case Token::Kind::Recursive:
                result = dispatchRecursive(ctx, tk, srcValue);
                break;
            case Token::Kind::Filter:
                result = dispatchFilter(ctx, tk, srcValue);
                break;
            case Token::Kind::KeyList:
                result = dispatchKeyList(ctx, tk, srcValue);
                break;
            default:
                result = std::unexpected(EvalError::TypeMismatchObject);
                break;
        }
        
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
>::dispatch<ConceptResultStreamer<ResultCollector>>(
    const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx,
    const QJsonArray& src, const ConceptResultStreamer<ResultCollector>& streamer);

} // namespace json_query::json_path::internal
