#pragma once

#include <QJsonArray>
#include <QJsonValue>
#include "JSONPathCompile.hpp"
#include "json-query/json-path/PathEvalCtx.hpp"

namespace json_query {
    class JSONPath; // Forward declaration
}

namespace json_query::json_path::detail {

/**
 * @brief Template function for evaluating tokens of different kinds
 * 
 * This template is specialized for each Token::Kind to provide
 * specific evaluation logic for different JSONPath token types.
 * 
 * @tparam K The Token::Kind to evaluate
 * @param ctx The PathEvalCtx instance (for context and options)
 * @param tk The token to evaluate
 * @param v The JSON value to evaluate against
 * @return QJsonArray containing all matching results
 */
template<Token::Kind K>
QJsonArray eval(const json_query::json_path::detail::PathEvalCtx& ctx,
                const Token& tk,
                const QJsonValue& v);

// Transitional wrapper keeping old signature functional until call sites migrate
template<Token::Kind K>
inline QJsonArray eval(const json_query::JSONPath& jp,
                       const Token& tk,
                       const QJsonValue& v)
{
    json_query::json_path::detail::PathEvalCtx ctx{jp.m_tokens, jp.m_filters, jp.m_option, jp.m_func};
    return eval<K>(ctx, tk, v);
}

// Explicit template specialization declarations
template<>
QJsonArray eval<Token::Kind::Key>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                  const Token& tk,
                                  const QJsonValue& v);

template<>
QJsonArray eval<Token::Kind::Index>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                    const Token& tk,
                                    const QJsonValue& v);

template<>
QJsonArray eval<Token::Kind::Slice>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                    const Token& tk,
                                    const QJsonValue& v);

template<>
QJsonArray eval<Token::Kind::Wildcard>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                       const Token& tk,
                                       const QJsonValue& v);

template<>
QJsonArray eval<Token::Kind::Recursive>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                        const Token& tk,
                                        const QJsonValue& v);

template<>
QJsonArray eval<Token::Kind::Filter>(const json_query::json_path::detail::PathEvalCtx& ctx,
                                     const Token& tk,
                                     const QJsonValue& v);

} // namespace json_query::json_path::detail
