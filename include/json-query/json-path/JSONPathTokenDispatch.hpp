#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <expected>

#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"

// Forward declarations for TableGen architecture
namespace json_query::json_path::internal {

// Enum for different error handling strategies
enum class ErrorHandlingStrategy {
    PermissiveDirectAccess,      // Direct array access (tokenPos == 1)
    PermissiveRecursiveContext,  // After recursive descent
    StrictPropertyChain,         // After property chain access
    DefaultPermissive            // Default permissive handling
};

// Template for error handling strategy definitions
template<ErrorHandlingStrategy Strategy>
struct ErrorHandlingDef {
    static constexpr bool enabled = false;
};

// Template specializations for each error handling strategy
template<>
struct ErrorHandlingDef<ErrorHandlingStrategy::PermissiveDirectAccess> {
    static constexpr bool enabled = true;
    
    static bool matches(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx) {
        return tk.kind == Token::Kind::Index && tokenPos == 1;
    }
};

template<>
struct ErrorHandlingDef<ErrorHandlingStrategy::PermissiveRecursiveContext> {
    static constexpr bool enabled = true;
    
    static bool matches(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx) {
        if (tk.kind != Token::Kind::Index || tokenPos <= 1) return false;
        
        // Check if we're in a recursive descent context by looking back through tokens
        for (qsizetype i = tokenPos - 1; i >= 1; --i) {
            const Token& prevToken = ctx.tokens[i];
            if (prevToken.kind == Token::Kind::Recursive) {
                return true; // Found recursive descent in the token chain
            } else if (prevToken.kind == Token::Kind::Key) {
                return false; // Property access breaks the recursive context
            }
            // Index tokens continue the recursive context
        }
        return false;
    }
};

template<>
struct ErrorHandlingDef<ErrorHandlingStrategy::StrictPropertyChain> {
    static constexpr bool enabled = true;
    
    static bool matches(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx) {
        if (tk.kind != Token::Kind::Index || tokenPos <= 1) return false;
        
        // Check if we're NOT in a recursive context (property chain access)
        for (qsizetype i = tokenPos - 1; i >= 1; --i) {
            const Token& prevToken = ctx.tokens[i];
            if (prevToken.kind == Token::Kind::Recursive) {
                return false; // Found recursive descent - not a property chain
            } else if (prevToken.kind == Token::Kind::Key) {
                return true; // Property access - this is a property chain
            }
        }
        return false;
    }
};

template<>
struct ErrorHandlingDef<ErrorHandlingStrategy::DefaultPermissive> {
    static constexpr bool enabled = true;
    
    static bool matches(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx) {
        return true; // Default fallback strategy
    }
};

// Template for error handling strategy implementations
template<ErrorHandlingStrategy Strategy>
struct ErrorHandlingProcessor {
    template<typename StreamerType>
    static void processFailure(const Token& tk, EvalError error, const StreamerType& streamer, 
                              bool& anySuccess, EvalError& lastError) = delete;
    
    template<typename StreamerType>
    static void processFinalResult(const Token& tk, const StreamerType& streamer, 
                                  bool anySuccess, EvalError lastError) = delete;
};

// Specialization for Permissive Direct Access processing
template<>
struct ErrorHandlingProcessor<ErrorHandlingStrategy::PermissiveDirectAccess> {
    template<typename StreamerType>
    static void processFailure(const Token& tk, EvalError error, const StreamerType& streamer, 
                              bool& anySuccess, EvalError& lastError) {
        lastError = error;
        // Continue processing - permissive error handling
    }
    
    template<typename StreamerType>
    static void processFinalResult(const Token& tk, const StreamerType& streamer, 
                                  bool anySuccess, EvalError lastError) {
        // For permissive error handling, don't emit error if no results (RFC 9535 compliance)
        // Empty result for RFC 9535 compliance - no emission needed
    }
};

// Specialization for Permissive Recursive Context processing
template<>
struct ErrorHandlingProcessor<ErrorHandlingStrategy::PermissiveRecursiveContext> {
    template<typename StreamerType>
    static void processFailure(const Token& tk, EvalError error, const StreamerType& streamer, 
                              bool& anySuccess, EvalError& lastError) {
        lastError = error;
        // Continue processing - permissive error handling after recursive descent
    }
    
    template<typename StreamerType>
    static void processFinalResult(const Token& tk, const StreamerType& streamer, 
                                  bool anySuccess, EvalError lastError) {
        // For permissive error handling, don't emit error if no results (RFC 9535 compliance)
        // Empty result for RFC 9535 compliance - no emission needed
    }
};

// Specialization for Strict Property Chain processing
template<>
struct ErrorHandlingProcessor<ErrorHandlingStrategy::StrictPropertyChain> {
    template<typename StreamerType>
    static void processFailure(const Token& tk, EvalError error, const StreamerType& streamer, 
                              bool& anySuccess, EvalError& lastError) {
        lastError = error;
        // Strict error handling: propagate errors immediately (property chain access)
        streamer.handleError(lastError);
        // Note: This should cause early return in the calling function
    }
    
    template<typename StreamerType>
    static void processFinalResult(const Token& tk, const StreamerType& streamer, 
                                  bool anySuccess, EvalError lastError) {
        // For strict error handling, emit error if no results
        if (!anySuccess) {
            streamer.handleError(lastError);
        }
    }
};

// Specialization for Default Permissive processing
template<>
struct ErrorHandlingProcessor<ErrorHandlingStrategy::DefaultPermissive> {
    template<typename StreamerType>
    static void processFailure(const Token& tk, EvalError error, const StreamerType& streamer, 
                              bool& anySuccess, EvalError& lastError) {
        lastError = error;
        // Continue processing - default permissive error handling
    }
    
    template<typename StreamerType>
    static void processFinalResult(const Token& tk, const StreamerType& streamer, 
                                  bool anySuccess, EvalError lastError) {
        // Only emit error if ALL evaluations failed (non-permissive contexts)
        if (!anySuccess) {
            streamer.handleError(lastError);
        }
    }
};

// TableGen-inspired recursive dispatch table for error handling strategies
template<ErrorHandlingStrategy... Strategies>
struct ErrorHandlingDispatchTable;

template<ErrorHandlingStrategy FirstStrategy, ErrorHandlingStrategy... RestStrategies>
struct ErrorHandlingDispatchTable<FirstStrategy, RestStrategies...> {
    template<typename StreamerType>
    static bool dispatch(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx,
                        const QJsonArray& src, const StreamerType& streamer);

private:
    template<ErrorHandlingStrategy Strategy, typename StreamerType>
    static bool processWithStrategy(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx,
                                   const QJsonArray& src, const StreamerType& streamer);
};

template<>
struct ErrorHandlingDispatchTable<> {
    template<typename StreamerType>
    static bool dispatch(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx,
                        const QJsonArray& src, const StreamerType& streamer) {
        // Should never reach here with proper strategy coverage
        return false;
    }
};

// Compile-time dispatch table with prioritized strategy ordering
using ErrorHandlingDispatcher = ErrorHandlingDispatchTable<
    ErrorHandlingStrategy::PermissiveDirectAccess,
    ErrorHandlingStrategy::PermissiveRecursiveContext,
    ErrorHandlingStrategy::StrictPropertyChain,
    ErrorHandlingStrategy::DefaultPermissive
>;

} // namespace json_query::json_path::internal

namespace json_query::json_path::detail {

// Forward declarations
using json_query::json_path::Token;
using json_query::json_path::EvalError;
using json_query::json_path::detail::PathEvalCtx;
using json_query::json_path::internal::ResultStreamer;

// ---------------------------------------------------------------------------
//  Token evaluation dispatch system
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> evaluateTokenExpected(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);

template<typename StreamerType>
void fanOutStreamingImpl(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, 
                        const StreamerType& streamer, qsizetype tokenPos)
{
    // Use TableGen-inspired error handling dispatch
    internal::ErrorHandlingDispatcher::dispatch(tk, tokenPos, ctx, src, streamer);
}

// Legacy overload for backward compatibility with ResultStreamer
void fanOutStreaming(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, 
                    const ResultStreamer& streamer, qsizetype tokenPos);

// Template version for concept-based streamers
template<typename StreamerType>
void fanOutStreaming(const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, 
                    const StreamerType& streamer, qsizetype tokenPos = -1)
{
    fanOutStreamingImpl(ctx, tk, src, streamer, tokenPos);
}

} // namespace json_query::json_path::detail

namespace json_query::json_path::internal {

// ---------------------------------------------------------------------------
//  TableGen-inspired token dispatch function declarations
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
