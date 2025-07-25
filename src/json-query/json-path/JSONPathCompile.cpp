#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathParseUtils.hpp"
#include "json-query/json-path/JSONPathBracketRules.hpp"
#include "json-query/json-path/JSONPathParsers.hpp"

#include <iostream>
#include <QDebug>
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/JSONPathHelpers.hpp"

#include "json-query/json-path/internal/QtHash.hpp"
#include "json-query/utils/JSONQueryUtils.hpp"
#include "json-query/json-path/JSONPathFilter.hpp"  // For compileFilter implementation

#include <limits>
#include <ctre.hpp>

namespace json_query::json_path
{

// ──────────────────────────────────────────────────────────────────────
//  detectTrailingFunction - moved from JSONPath
// ──────────────────────────────────────────────────────────────────────
json_query::json_path::FunctionType detectTrailingFunction(QString& path)
{
    using enum json_query::json_path::FunctionType;

    static const QPair<QString, json_query::json_path::FunctionType> table[] = {
        {".length()", Length},
        {".min()", Min},
        {".max()", Max},
    };

    for (auto& p : table)
        if (path.endsWith(p.first))
        {
            path.chop(p.first.size());
            return p.second;
        }

    return None;
}

// ──────────────────────────────────────────────────────────────────────
//  compilePath - moved from JSONPath
// ──────────────────────────────────────────────────────────────────────
std::expected<json_query::json_path::Compiled, json_query::json_path::Error> compilePath(QStringView sv)
{
    qCDebug(json_query::json_path::jsonPathLog) << "compilePath() sv=" << sv;
    using K = json_query::json_path::Token::Kind;
    QVector<json_query::json_path::Token> tokens;
    QVector<json_query::json_path::ContextFilterFn> contextFilters;
    QVector<json_query::json_path::FilterFn> filters;
    json_query::json_path::detail::KeyBuilder kb{tokens};

    if (sv.isEmpty() || sv[0] != u'$')
        return std::unexpected(json_query::json_path::Error::MissingRoot);
    tokens.append(json_query::json_path::Token{ json_query::json_path::Token::Kind::Key, 0, {}, qt_hash(sv.first(1)),
                                                sv.first(1).toString() });

    if (sv.size() > 1 && sv[1] != u'.' && sv[1] != u'[')
        return std::unexpected(json_query::json_path::Error::UnexpectedAfterRoot);

    // C++23 Monadic Chain - Functional composition for parsing loop
    // Transform imperative loop into elegant monadic fold operation
    struct ParseState {
        qsizetype pos;
        QVector<json_query::json_path::Token>& tokens;
        QVector<json_query::json_path::ContextFilterFn>& contextFilters;
        QVector<json_query::json_path::FilterFn>& filters;
        json_query::json_path::detail::KeyBuilder& kb;
        QStringView sv;
    };
    
    ParseState state{1, tokens, contextFilters, filters, kb, sv};
    
    // Monadic fold: repeatedly apply parser until end of input or error
    return std::expected<ParseState, json_query::json_path::Error>{std::move(state)}
        .and_then([](ParseState&& state) -> std::expected<ParseState, json_query::json_path::Error> {
            // Recursive monadic parser application
            std::function<std::expected<ParseState, json_query::json_path::Error>(ParseState&&)> parseNext = 
                [&parseNext](ParseState&& currentState) -> std::expected<ParseState, json_query::json_path::Error> {
                
                // Base case: reached end of input
                if (currentState.pos >= currentState.sv.size()) {
                    return std::move(currentState);
                }
                
                // Apply appropriate parser based on current character
                auto nextPosResult = 
                    (currentState.sv[currentState.pos] == u'.' && 
                     currentState.pos + 1 < currentState.sv.size() && 
                     currentState.sv[currentState.pos + 1] == u'.') ? 
                        // Handle descendant segment (..) directly
                        [&currentState]() -> std::expected<qsizetype, json_query::json_path::Error> {
                            qCDebug(json_query::json_path::jsonPathLog) << "compilePath: found descendant segment (..) at pos=" << currentState.pos;
                            currentState.tokens.append(json_query::json_path::Token{json_query::json_path::Token::Kind::Recursive});
                            qsizetype newPos = currentState.pos + 2;
                            if (newPos >= currentState.sv.size()) {
                                return std::unexpected(json_query::json_path::Error::TrailingRecursive);
                            }
                            return newPos;
                        }()
                  : (currentState.sv[currentState.pos] == u'*') ?
                        // Handle wildcard directly
                        [&currentState]() -> std::expected<qsizetype, json_query::json_path::Error> {
                            qCDebug(json_query::json_path::jsonPathLog) << "compilePath: found wildcard (*) at pos=" << currentState.pos;
                            currentState.tokens.append(json_query::json_path::Token{json_query::json_path::Token::Kind::Wildcard});
                            return currentState.pos + 1;
                        }()
                  : (currentState.sv[currentState.pos] == u'.') ? 
                        json_query::json_path::detail::parseDot(currentState.pos, currentState.sv, currentState.kb, currentState.tokens)
                  : (currentState.sv[currentState.pos] == u'[') ? 
                        json_query::json_path::detail::parseBracket(currentState.pos, currentState.sv, currentState.kb, currentState.tokens, 
                                           currentState.contextFilters, currentState.filters)
                  :     json_query::json_path::detail::parseBare(currentState.pos, currentState.sv, currentState.kb);
                
                // Monadic composition: chain parser result with recursive call
                return nextPosResult
                    .and_then([&parseNext, currentState = std::move(currentState)](qsizetype nextPos) mutable -> std::expected<ParseState, json_query::json_path::Error> {
                        currentState.pos = nextPos;
                        return parseNext(std::move(currentState));
                    })
                    .or_else([](json_query::json_path::Error error) -> std::expected<ParseState, json_query::json_path::Error> {
                        qCDebug(json_query::json_path::jsonPathLog) << "compilePath: parser returned error" << static_cast<int>(error);
                        return std::unexpected(error);
                    });
            };
            
            return parseNext(std::move(state));
        })
        .transform([](ParseState&& finalState) -> json_query::json_path::Compiled {
            qCDebug(json_query::json_path::jsonPathLog) << "compilePath: monadic parsing completed successfully";
            return json_query::json_path::Compiled{ 
                std::move(finalState.tokens), 
                std::move(finalState.filters), 
                std::move(finalState.contextFilters) 
            };
        });
}

// ──────────────────────────────────────────────────────────────────────
//  High-level compile function
// ──────────────────────────────────────────────────────────────────────
std::expected<json_query::json_path::CompilationResult, json_query::json_path::Error> compile(QStringView rawPath)
{
    qCDebug(json_query::json_path::jsonPathLog) << "compile() rawPath=" << rawPath;
    QString path = rawPath.toString();
    
    // Extract any trailing function → updates `path` and yields `func`
    json_query::json_path::FunctionType func = json_query::json_path::detectTrailingFunction(path);

    if (path.isEmpty())
        return std::unexpected(json_query::json_path::Error::EmptySegment);

    // C++23 Monadic Chain - Elegant error composition without manual checks!
    return json_query::json_path::compilePath(path)
        .transform([func](json_query::json_path::Compiled&& compiled) -> json_query::json_path::CompilationResult {
            qCDebug(json_query::json_path::jsonPathLog) << "compile: compilePath succeeded";
            return json_query::json_path::CompilationResult{
                func,
                std::move(compiled)
            };
        })
        .or_else([](json_query::json_path::Error error) -> std::expected<json_query::json_path::CompilationResult, json_query::json_path::Error> {
            qCDebug(json_query::json_path::jsonPathLog) << "compile: compilePath failed with error" << static_cast<int>(error);
            return std::unexpected(error);
        });
}

} // namespace json_query::json_path
