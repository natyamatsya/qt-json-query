#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <expected>
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"

// Forward declarations for TableGen architecture
namespace json_query::json_path::internal
{

// Enum for different error handling strategies
enum class ErrorHandlingStrategy
{
    PermissiveDirectAccess,     // Direct array access (tokenPos == 1)
    PermissiveRecursiveContext, // After recursive descent
    StrictPropertyChain,        // After property chain access
    DefaultPermissive           // Default permissive handling
};

// Template for error handling strategy definitions
template <ErrorHandlingStrategy Strategy>
struct ErrorHandlingDef
{
    static constexpr bool enabled = false;
};

// Template specializations for each error handling strategy
template <>
struct ErrorHandlingDef<ErrorHandlingStrategy::PermissiveDirectAccess>
{
    static constexpr bool enabled = true;

    static bool matches(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx)
    {
        return tk.kind == Token::Kind::Index && tokenPos == 1;
    }
};

template <>
struct ErrorHandlingDef<ErrorHandlingStrategy::PermissiveRecursiveContext>
{
    static constexpr bool enabled = true;

    static bool matches(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx)
    {
        if (tk.kind != Token::Kind::Index || tokenPos <= 1)
            return false;

        // Check if we're in a recursive descent context by looking back through tokens
        for (qsizetype i = tokenPos - 1; i >= 1; --i)
        {
            const Token& prevToken = ctx.tokens[i];
            if (prevToken.kind == Token::Kind::Recursive)
                return true; // Found recursive descent in the token chain
            else if (prevToken.kind == Token::Kind::Key)
                return false; // Property access breaks the recursive context
            // Index tokens continue the recursive context
        }
        return false;
    }
};

template <>
struct ErrorHandlingDef<ErrorHandlingStrategy::StrictPropertyChain>
{
    static constexpr bool enabled = true;

    static bool matches(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx)
    {
        if (tk.kind != Token::Kind::Index || tokenPos <= 1)
            return false;

        // Check if we're NOT in a recursive context (property chain access)
        for (qsizetype i = tokenPos - 1; i >= 1; --i)
        {
            const Token& prevToken = ctx.tokens[i];
            if (prevToken.kind == Token::Kind::Recursive)
                return false; // Found recursive descent - not a property chain
            else if (prevToken.kind == Token::Kind::Key)
                return true; // Property access - this is a property chain
        }
        return false;
    }
};

template <>
struct ErrorHandlingDef<ErrorHandlingStrategy::DefaultPermissive>
{
    static constexpr bool enabled = true;

    static bool matches(const Token& tk, qsizetype tokenPos, const detail::PathEvalCtx& ctx)
    {
        return true; // Default fallback strategy
    }
};

// Template for error handling strategy implementations
template <ErrorHandlingStrategy Strategy>
struct ErrorHandlingProcessor
{
    template <internal::ResultStreamerConcept StreamerType>
    static void processFailure(const Token&        tk,
                               EvalError           error,
                               const StreamerType& streamer,
                               bool&               anySuccess,
                               EvalError&          lastError) = delete;

    template <internal::ResultStreamerConcept StreamerType>
    static void
    processFinalResult(const Token& tk, const StreamerType& streamer, bool anySuccess, EvalError lastError) = delete;
};

// Specialization for Permissive Direct Access processing
template <>
struct ErrorHandlingProcessor<ErrorHandlingStrategy::PermissiveDirectAccess>
{
    template <internal::ResultStreamerConcept StreamerType>
    static void processFailure(
        const Token& tk, EvalError error, const StreamerType& streamer, bool& anySuccess, EvalError& lastError)
    {
        lastError = error;
        // Continue processing - permissive error handling
    }

    template <internal::ResultStreamerConcept StreamerType>
    static void processFinalResult(const Token& tk, const StreamerType& streamer, bool anySuccess, EvalError lastError)
    {
        // For direct access, we don't emit anything on failure
    }
};

// Specialization for Permissive Recursive Context processing
template <>
struct ErrorHandlingProcessor<ErrorHandlingStrategy::PermissiveRecursiveContext>
{
    template <internal::ResultStreamerConcept StreamerType>
    static void processFailure(
        const Token& tk, EvalError error, const StreamerType& streamer, bool& anySuccess, EvalError& lastError)
    {
        lastError = error;
        // Continue processing - permissive error handling
    }

    template <internal::ResultStreamerConcept StreamerType>
    static void processFinalResult(const Token& tk, const StreamerType& streamer, bool anySuccess, EvalError lastError)
    {
        // For recursive context, we don't emit anything on failure
    }
};

// Specialization for Strict Property Chain processing
template <>
struct ErrorHandlingProcessor<ErrorHandlingStrategy::StrictPropertyChain>
{
    template <internal::ResultStreamerConcept StreamerType>
    static void processFailure(
        const Token& tk, EvalError error, const StreamerType& streamer, bool& anySuccess, EvalError& lastError)
    {
        lastError = error;
        if (streamer.canHandleErrors())
            streamer.handleError(error);
    }

    template <internal::ResultStreamerConcept StreamerType>
    static void processFinalResult(const Token& tk, const StreamerType& streamer, bool anySuccess, EvalError lastError)
    {
        // For strict property chains, we propagate errors
        if (!anySuccess && streamer.canHandleErrors())
            streamer.handleError(lastError);
    }
};

// Specialization for Default Permissive processing
template <>
struct ErrorHandlingProcessor<ErrorHandlingStrategy::DefaultPermissive>
{
    template <internal::ResultStreamerConcept StreamerType>
    static void processFailure(
        const Token& tk, EvalError error, const StreamerType& streamer, bool& anySuccess, EvalError& lastError)
    {
        lastError = error;
        // Default permissive - continue processing
    }

    template <internal::ResultStreamerConcept StreamerType>
    static void processFinalResult(const Token& tk, const StreamerType& streamer, bool anySuccess, EvalError lastError)
    {
        // Default permissive - no special error handling
    }
};

// TableGen-inspired recursive dispatch table for error handling strategies
template <ErrorHandlingStrategy... Strategies>
struct ErrorHandlingDispatchTable;

template <ErrorHandlingStrategy FirstStrategy, ErrorHandlingStrategy... RestStrategies>
struct ErrorHandlingDispatchTable<FirstStrategy, RestStrategies...>
{
    template <internal::ResultStreamerConcept StreamerType>
    static bool dispatch(const Token&               tk,
                         qsizetype                  tokenPos,
                         const detail::PathEvalCtx& ctx,
                         const QJsonArray&          src,
                         const StreamerType&        streamer);

  private:
    template <ErrorHandlingStrategy Strategy, internal::ResultStreamerConcept StreamerType>
    static bool processWithStrategy(const Token&               tk,
                                    qsizetype                  tokenPos,
                                    const detail::PathEvalCtx& ctx,
                                    const QJsonArray&          src,
                                    const StreamerType&        streamer);
};

template <>
struct ErrorHandlingDispatchTable<>
{
    template <internal::ResultStreamerConcept StreamerType>
    static bool dispatch(const Token&               tk,
                         qsizetype                  tokenPos,
                         const detail::PathEvalCtx& ctx,
                         const QJsonArray&          src,
                         const StreamerType&        streamer)
    {
        // Should never reach here with proper strategy coverage
        return false;
    }
};

// Compile-time dispatch table with prioritized strategy ordering
using ErrorHandlingDispatcher = ErrorHandlingDispatchTable<ErrorHandlingStrategy::PermissiveDirectAccess,
                                                           ErrorHandlingStrategy::PermissiveRecursiveContext,
                                                           ErrorHandlingStrategy::StrictPropertyChain,
                                                           ErrorHandlingStrategy::DefaultPermissive>;

} // namespace json_query::json_path::internal

namespace json_query::json_path::detail
{

// Forward declarations
using json_query::json_path::EvalError;
using json_query::json_path::Token;
using json_query::json_path::detail::PathEvalCtx;

// ---------------------------------------------------------------------------
//  Token evaluation dispatch system
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> evaluateToken(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);

template <internal::ResultStreamerConcept StreamerType>
void fanOutStreamingImpl(
    const PathEvalCtx& ctx, const Token& tk, const QJsonArray& src, const StreamerType& streamer, qsizetype tokenPos)
{
    // Use TableGen-inspired error handling dispatch
    internal::ErrorHandlingDispatcher::dispatch(tk, tokenPos, ctx, src, streamer);
}

} // namespace json_query::json_path::detail

namespace json_query::json_path::internal
{

// ---------------------------------------------------------------------------
//  TableGen-inspired token dispatch function declarations
// ---------------------------------------------------------------------------

// Individual token type dispatchers
std::expected<QJsonArray, EvalError> dispatchKey(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError>
dispatchIndex(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError>
dispatchSlice(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError>
dispatchWildcard(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError>
dispatchRecursive(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError>
dispatchFilter(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError>
dispatchKeyList(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);

} // namespace json_query::json_path::internal

// Include inline implementations for critical hot path functions
#include "internal/JSONPathTokenDispatch.inl"
