// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/TypedTokenEvaluators.hpp"
#include <expected>
#include <type_traits>
#include <array>

namespace json_query::json_path::internal
{

/**
 * @brief Compile-time token dispatch system for eliminating runtime switch overhead
 *
 * This system replaces the runtime switch(tk.kind) with compile-time template
 * dispatch, providing significant performance improvements by eliminating branch
 * misprediction penalties and enabling better compiler optimizations.
 */

// Forward declarations for dispatch functions
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

/**
 * @brief Template-based dispatch function pointer table
 *
 * Maps Token::Kind enum values to their corresponding dispatch functions
 * at compile time, eliminating runtime switch overhead.
 */
template <Token::Kind Kind>
struct TokenDispatchTraits
{
    static constexpr auto dispatch_function = nullptr;
};

// Template specializations for each token kind
template <>
struct TokenDispatchTraits<Token::Kind::Key>
{
    static constexpr auto dispatch_function = &dispatchKey;
};

template <>
struct TokenDispatchTraits<Token::Kind::Index>
{
    static constexpr auto dispatch_function = &dispatchIndex;
};

template <>
struct TokenDispatchTraits<Token::Kind::Slice>
{
    static constexpr auto dispatch_function = &dispatchSlice;
};

template <>
struct TokenDispatchTraits<Token::Kind::Wildcard>
{
    static constexpr auto dispatch_function = &dispatchWildcard;
};

template <>
struct TokenDispatchTraits<Token::Kind::Recursive>
{
    static constexpr auto dispatch_function = &dispatchRecursive;
};

template <>
struct TokenDispatchTraits<Token::Kind::Filter>
{
    static constexpr auto dispatch_function = &dispatchFilter;
};

template <>
struct TokenDispatchTraits<Token::Kind::KeyList>
{
    static constexpr auto dispatch_function = &dispatchKeyList;
};

/**
 * @brief Compile-time dispatch table using constexpr array
 *
 * Creates a lookup table that maps Token::Kind enum values to function pointers
 * at compile time, enabling O(1) dispatch without runtime branching.
 */
class CompileTimeTokenDispatcher
{
  public:
    using DispatchFunction = std::expected<QJsonArray, EvalError> (*)(const detail::PathEvalCtx&,
                                                                      const Token&,
                                                                      const QJsonValue&);

  private:
    // Compile-time dispatch table construction
    static constexpr size_t TOKEN_KIND_COUNT = 7; // Number of Token::Kind enum values

    static constexpr DispatchFunction getDispatchFunction(Token::Kind kind)
    {
        switch (kind)
        {
        case Token::Kind::Key:
            return TokenDispatchTraits<Token::Kind::Key>::dispatch_function;
        case Token::Kind::Index:
            return TokenDispatchTraits<Token::Kind::Index>::dispatch_function;
        case Token::Kind::Slice:
            return TokenDispatchTraits<Token::Kind::Slice>::dispatch_function;
        case Token::Kind::Wildcard:
            return TokenDispatchTraits<Token::Kind::Wildcard>::dispatch_function;
        case Token::Kind::Recursive:
            return TokenDispatchTraits<Token::Kind::Recursive>::dispatch_function;
        case Token::Kind::Filter:
            return TokenDispatchTraits<Token::Kind::Filter>::dispatch_function;
        case Token::Kind::KeyList:
            return TokenDispatchTraits<Token::Kind::KeyList>::dispatch_function;
        default:
            return nullptr;
        }
    }

  public:
    /**
     * @brief Fast compile-time token dispatch with type specialization
     *
     * Combines token dispatch elimination with type specialization for maximum
     * compile-time optimization and minimal runtime overhead.
     *
     * @param ctx Path evaluation context
     * @param tk Token to dispatch
     * @param v JSON value to process
     * @return Expected result or error
     */
    inline static std::expected<QJsonArray, EvalError>
    dispatch(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
    {
        // Use type-specialized evaluators for common token types
        switch (tk.kind)
        {
        case Token::Kind::Key:
        case Token::Kind::Index:
        case Token::Kind::Wildcard:
            // Use enhanced type-aware dispatch for maximum optimization
            return TypedCompileTimeTokenDispatcher::dispatch(ctx, tk, v);

        default:
            // Fallback to original dispatch for other token types
            const auto dispatch_fn{getDispatchFunction(tk.kind)};
            if (dispatch_fn == nullptr) [[unlikely]]
                return std::unexpected(EvalError::TypeMismatchObject);
            return dispatch_fn(ctx, tk, v);
        }
    }
};

/**
 * @brief Template-based compile-time dispatch for known token kinds
 *
 * When the token kind is known at compile time, this provides even better
 * optimization by completely eliminating the table lookup.
 */
template <Token::Kind Kind>
inline std::expected<QJsonArray, EvalError>
dispatchTokenKind(const detail::PathEvalCtx& ctx, const Token& tk, const QJsonValue& v) noexcept
{
    static_assert(TokenDispatchTraits<Kind>::dispatch_function != nullptr,
                  "Invalid token kind for compile-time dispatch");

    // Direct function call with zero runtime overhead
    return TokenDispatchTraits<Kind>::dispatch_function(ctx, tk, v);
}

} // namespace json_query::json_path::internal
