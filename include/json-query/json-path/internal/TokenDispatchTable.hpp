#pragma once

#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <expected>
#include <array>

namespace json_query::json_path::internal
{

// Forward declare the actual evaluator functions from JSONPathTokenEvaluators.hpp
namespace json_path = json_query::json_path;
using PathEvalCtx   = detail::PathEvalCtx;

/**
 * @brief TableGen-inspired token metadata
 */
struct TokenMetadata
{
    Token::Kind kind;
    const char* name;
    bool        requiresContext;
    bool        canMultiply;
    bool        isTerminal;
    int         priority;
};

/**
 * @brief TableGen-inspired dispatch function type
 */
using TokenEvaluatorFn = std::expected<QJsonArray, EvalError> (*)(const PathEvalCtx&, const Token&, const QJsonValue&);

// Forward declare the wrapper functions that will be implemented in the .cpp file
std::expected<QJsonArray, EvalError> dispatchKey(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchIndex(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchSlice(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchWildcard(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchRecursive(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchFilter(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);
std::expected<QJsonArray, EvalError> dispatchKeyList(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v);

// Global dispatch and metadata tables (TableGen-inspired declarative style)
constexpr std::array<TokenEvaluatorFn, 7> TOKEN_DISPATCH_TABLE = {
    dispatchKey,       // 0: Token::Kind::Key
    dispatchKeyList,   // 1: Token::Kind::KeyList
    dispatchIndex,     // 2: Token::Kind::Index
    dispatchSlice,     // 3: Token::Kind::Slice
    dispatchWildcard,  // 4: Token::Kind::Wildcard
    dispatchRecursive, // 5: Token::Kind::Recursive
    dispatchFilter     // 6: Token::Kind::Filter
};

constexpr std::array<TokenMetadata, 7> TOKEN_METADATA_TABLE = {
    TokenMetadata{Token::Kind::Key, "key", false, false, false, 100},
    TokenMetadata{Token::Kind::KeyList, "keylist", false, true, false, 40},
    TokenMetadata{Token::Kind::Index, "index", false, false, false, 90},
    TokenMetadata{Token::Kind::Slice, "slice", false, true, false, 80},
    TokenMetadata{Token::Kind::Wildcard, "wildcard", false, true, false, 70},
    TokenMetadata{Token::Kind::Recursive, "recursive", true, true, false, 60},
    TokenMetadata{Token::Kind::Filter, "filter", true, true, false, 50}};

/**
 * @brief TableGen-inspired token dispatcher
 *
 * Provides O(1) token evaluation dispatch using compile-time generated tables.
 * This replaces the manual switch statement with a declarative, extensible system.
 */
class TokenDispatcher
{
  public:
    /**
     * @brief Dispatch token evaluation using compile-time table
     */
    static std::expected<QJsonArray, EvalError> dispatch(const PathEvalCtx& ctx, const Token& tk, const QJsonValue& v)
    {

        const auto kindIndex = static_cast<std::size_t>(tk.kind);
        if (kindIndex >= TOKEN_DISPATCH_TABLE.size())
            return std::unexpected(EvalError::TypeMismatchObject);

        return TOKEN_DISPATCH_TABLE[kindIndex](ctx, tk, v);
    }

    /**
     * @brief Get token metadata for introspection
     */
    static constexpr const TokenMetadata& getMetadata(Token::Kind kind)
    {
        const auto kindIndex = static_cast<std::size_t>(kind);
        return TOKEN_METADATA_TABLE[kindIndex];
    }

    /**
     * @brief Check if token requires context
     */
    static constexpr bool requiresContext(Token::Kind kind) { return getMetadata(kind).requiresContext; }

    /**
     * @brief Check if token can multiply results
     */
    static constexpr bool canMultiply(Token::Kind kind) { return getMetadata(kind).canMultiply; }

    /**
     * @brief Get token name for debugging
     */
    static constexpr const char* getName(Token::Kind kind) { return getMetadata(kind).name; }

    /**
     * @brief Get token priority for optimization
     */
    static constexpr int getPriority(Token::Kind kind) { return getMetadata(kind).priority; }
};

/**
 * @brief Compile-time token sequence optimizer
 *
 * Provides compile-time optimization hints for token sequences.
 */
template <Token::Kind K1, Token::Kind K2>
constexpr bool canOptimizeSequence()
{
    // Example: consecutive wildcards can be optimized
    if constexpr (K1 == Token::Kind::Wildcard && K2 == Token::Kind::Wildcard)
        return true;
    // Example: key followed by index can use direct access
    if constexpr (K1 == Token::Kind::Key && K2 == Token::Kind::Index)
        return true;
    return false;
}

} // namespace json_query::json_path::internal
