#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include <expected>
#include "JSONPathCompile.hpp"
#include "JSONPathEvalError.hpp"
#include "internal/PathEvalCtx.hpp"

namespace json_query {
    class JSONPath; // Forward declaration
}

namespace json_query::json_path::detail {

/**
 * @brief Template function for evaluating tokens with std::expected error handling
 * 
 * This template is specialized for each Token::Kind to provide
 * specific evaluation logic with proper error reporting using std::expected.
 * 
 * @tparam K The Token::Kind to evaluate
 * @param ctx The PathEvalCtx instance (for context and options)
 * @param tk The token to evaluate
 * @param v The JSON value to evaluate against
 * @return std::expected<QJsonArray, EvalError> containing results or error
 */
template<Token::Kind K>
std::expected<QJsonArray, EvalError> evalExpected(const json_query::json_path::detail::PathEvalCtx& ctx,
                                                   const Token& tk,
                                                   const QJsonValue& v);

// Explicit template specialization declarations for evalExpected
template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Key>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                                                     const Token& tk,
                                                                     const QJsonValue& v);

template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Index>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                                                       const Token& tk,
                                                                       const QJsonValue& v);

template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Slice>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                                                       const Token& tk,
                                                                       const QJsonValue& v);

template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Wildcard>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                                                          const Token& tk,
                                                                          const QJsonValue& v);

template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Recursive>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                                                           const Token& tk,
                                                                           const QJsonValue& v);

template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::Filter>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                                                        const Token& tk,
                                                                        const QJsonValue& v);

template<>
std::expected<QJsonArray, EvalError> evalExpected<Token::Kind::KeyList>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                                                         const Token& tk,
                                                                         const QJsonValue& v);

} // namespace json_query::json_path::detail
