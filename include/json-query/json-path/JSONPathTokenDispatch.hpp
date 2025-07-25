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

// Template version for concept-based streamers
template<typename StreamerType>
void fanOutStreaming(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, 
                    const StreamerType& streamer, qsizetype tokenPos = -1)
{
    bool anySuccess = false;
    EvalError lastError = EvalError::TypeMismatchObject; // Default error
    
    // Determine if we should use permissive or strict error handling
    bool usePermissiveErrorHandling = false;
    
    if (tk.kind == Token::Kind::Index) {
        // For Index tokens, we need to determine the context:
        // 1. Direct array access (e.g., $[5]) - permissive (RFC 9535)
        // 2. After property chain (e.g., $.foo.bar[5]) - strict (UpstreamArrayIndexOOB)
        // 3. After recursive descent (e.g., $..[5]) - permissive (RFC 9535)
        
        // Check if we're in a recursive descent context by looking back through tokens
        bool inRecursiveContext = false;
        bool hasPropertyChain = false;
        
        for (qsizetype i = tokenPos - 1; i >= 1; --i) {
            const Token& prevToken = ctx.tokens[i];
            if (prevToken.kind == Token::Kind::Recursive) {
                // Found recursive descent in the token chain
                inRecursiveContext = true;
                break;
            } else if (prevToken.kind == Token::Kind::Key) {
                // Found property access - this indicates a property chain
                hasPropertyChain = true;
            }
            // Continue looking back for recursive descent
        }
        
        if (inRecursiveContext) {
            // After recursive descent: use permissive error handling (RFC 9535)
            usePermissiveErrorHandling = true;
        } else if (hasPropertyChain) {
            // After property chain without recursive descent: use strict error handling (UpstreamArrayIndexOOB)
            usePermissiveErrorHandling = false;
        } else {
            // Direct array access: use permissive error handling (RFC 9535)
            usePermissiveErrorHandling = true;
        }
    }

    for (const auto& srcValue : src) {
        auto result = evaluateTokenExpected(ctx, tk, srcValue);
        if (result) {
            // Success: stream results directly
            anySuccess = true;
            streamer.emitArray(*result);
        } else {
            // Failure: record error
            lastError = result.error();
            
            // Context-aware error handling for Index tokens
            if (tk.kind == Token::Kind::Index && !usePermissiveErrorHandling) {
                // Strict error handling: propagate errors immediately (property chain access)
                streamer.handleError(lastError);
                return;
            }
            // Permissive error handling: continue processing (direct access or after recursive descent)
        }
    }
    
    // For permissive error handling, don't emit error if no results (RFC 9535 compliance)
    if (tk.kind == Token::Kind::Index && usePermissiveErrorHandling && !anySuccess) {
        // Empty result for RFC 9535 compliance - no emission needed
        return;
    }
    
    // Only emit error if ALL evaluations failed (non-permissive contexts)
    if (!anySuccess) {
        streamer.handleError(lastError);
    }
}

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
