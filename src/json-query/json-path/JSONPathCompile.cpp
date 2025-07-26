#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathParseUtils.hpp"
#include "json-query/json-path/JSONPathBracketRules.hpp"
#include "json-query/json-path/JSONPathParsers.hpp"
#include "json-query/json-path/JSONPathFilterParsers.hpp"

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
//  TableGen-Inspired Character Parsing Architecture
// ──────────────────────────────────────────────────────────────────────

// Enum for different character parsing strategies
enum class CharacterParsingType {
    DescendantSegment,        // Handle descendant segment (..)
    WildcardCharacter,        // Handle wildcard (*)
    DotSegment,              // Handle dot segments (.)
    BracketSegment,          // Handle bracket segments ([])
    BareSegment              // Handle bare segments (fallback)
};

// Template for character parsing strategy definitions
template<CharacterParsingType Type>
struct CharacterParsingDef {
    static constexpr bool enabled = false;
};

// Template specializations for each parsing strategy
template<>
struct CharacterParsingDef<CharacterParsingType::DescendantSegment> {
    static constexpr bool enabled = true;
    
    static bool matches(QStringView sv, qsizetype pos) {
        return (sv[pos] == u'.' && 
                pos + 1 < sv.size() && 
                sv[pos + 1] == u'.');
    }
};

template<>
struct CharacterParsingDef<CharacterParsingType::WildcardCharacter> {
    static constexpr bool enabled = true;
    
    static bool matches(QStringView sv, qsizetype pos) {
        return sv[pos] == u'*';
    }
};

template<>
struct CharacterParsingDef<CharacterParsingType::DotSegment> {
    static constexpr bool enabled = true;
    
    static bool matches(QStringView sv, qsizetype pos) {
        return sv[pos] == u'.';
    }
};

template<>
struct CharacterParsingDef<CharacterParsingType::BracketSegment> {
    static constexpr bool enabled = true;
    
    static bool matches(QStringView sv, qsizetype pos) {
        return sv[pos] == u'[';
    }
};

template<>
struct CharacterParsingDef<CharacterParsingType::BareSegment> {
    static constexpr bool enabled = true;
    
    static bool matches(QStringView sv, qsizetype pos) {
        return true; // Default fallback strategy
    }
};

// Template for character parsing strategy implementations
template<CharacterParsingType Type>
struct CharacterParsingStrategy {
    static std::expected<qsizetype, json_query::json_path::Error> parse(
        qsizetype pos, QStringView sv, json_query::json_path::detail::KeyBuilder& kb, 
        QVector<json_query::json_path::Token>& tokens,
        QVector<json_query::json_path::ContextFilterFn>& contextFilters,
        QVector<json_query::json_path::FilterFn>& filters) = delete;
};

// Specialization for Descendant Segment parsing
template<>
struct CharacterParsingStrategy<CharacterParsingType::DescendantSegment> {
    static std::expected<qsizetype, json_query::json_path::Error> parse(
        qsizetype pos, QStringView sv, json_query::json_path::detail::KeyBuilder& kb, 
        QVector<json_query::json_path::Token>& tokens,
        QVector<json_query::json_path::ContextFilterFn>& contextFilters,
        QVector<json_query::json_path::FilterFn>& filters) {
        
        qCDebug(json_query::json_path::jsonPathLog) << "compilePath: found descendant segment (..) at pos=" << pos;
        tokens.append(json_query::json_path::Token{json_query::json_path::Token::Kind::Recursive});
        qsizetype newPos = pos + 2;
        if (newPos >= sv.size()) {
            return std::unexpected(json_query::json_path::Error::TrailingRecursive);
        }
        return newPos;
    }
};

// Specialization for Wildcard Character parsing
template<>
struct CharacterParsingStrategy<CharacterParsingType::WildcardCharacter> {
    static std::expected<qsizetype, json_query::json_path::Error> parse(
        qsizetype pos, QStringView sv, json_query::json_path::detail::KeyBuilder& kb, 
        QVector<json_query::json_path::Token>& tokens,
        QVector<json_query::json_path::ContextFilterFn>& contextFilters,
        QVector<json_query::json_path::FilterFn>& filters) {
        
        qCDebug(json_query::json_path::jsonPathLog) << "compilePath: found wildcard (*) at pos=" << pos;
        tokens.append(json_query::json_path::Token{json_query::json_path::Token::Kind::Wildcard});
        return pos + 1;
    }
};

// Specialization for Dot Segment parsing
template<>
struct CharacterParsingStrategy<CharacterParsingType::DotSegment> {
    static std::expected<qsizetype, json_query::json_path::Error> parse(
        qsizetype pos, QStringView sv, json_query::json_path::detail::KeyBuilder& kb, 
        QVector<json_query::json_path::Token>& tokens,
        QVector<json_query::json_path::ContextFilterFn>& contextFilters,
        QVector<json_query::json_path::FilterFn>& filters) {
        
        return json_query::json_path::detail::parseDot(pos, sv, kb, tokens);
    }
};

// Specialization for Bracket Segment parsing
template<>
struct CharacterParsingStrategy<CharacterParsingType::BracketSegment> {
    static std::expected<qsizetype, json_query::json_path::Error> parse(
        qsizetype pos, QStringView sv, json_query::json_path::detail::KeyBuilder& kb, 
        QVector<json_query::json_path::Token>& tokens,
        QVector<json_query::json_path::ContextFilterFn>& contextFilters,
        QVector<json_query::json_path::FilterFn>& filters) {
        
        return json_query::json_path::detail::parseBracket(pos, sv, kb, tokens, contextFilters, filters);
    }
};

// Specialization for Bare Segment parsing
template<>
struct CharacterParsingStrategy<CharacterParsingType::BareSegment> {
    static std::expected<qsizetype, json_query::json_path::Error> parse(
        qsizetype pos, QStringView sv, json_query::json_path::detail::KeyBuilder& kb, 
        QVector<json_query::json_path::Token>& tokens,
        QVector<json_query::json_path::ContextFilterFn>& contextFilters,
        QVector<json_query::json_path::FilterFn>& filters) {
        
        return json_query::json_path::detail::parseBare(pos, sv, kb);
    }
};

// TableGen-inspired recursive dispatch table for character parsing strategies
template<CharacterParsingType... Types>
struct CharacterParsingDispatchTable;

template<CharacterParsingType FirstType, CharacterParsingType... RestTypes>
struct CharacterParsingDispatchTable<FirstType, RestTypes...> {
    static std::expected<qsizetype, json_query::json_path::Error> dispatch(
        qsizetype pos, QStringView sv, json_query::json_path::detail::KeyBuilder& kb, 
        QVector<json_query::json_path::Token>& tokens,
        QVector<json_query::json_path::ContextFilterFn>& contextFilters,
        QVector<json_query::json_path::FilterFn>& filters) {
        
        if constexpr (CharacterParsingDef<FirstType>::enabled) {
            if (CharacterParsingDef<FirstType>::matches(sv, pos)) {
                return CharacterParsingStrategy<FirstType>::parse(pos, sv, kb, tokens, contextFilters, filters);
            }
        }
        
        // Try next strategy in the dispatch table
        return CharacterParsingDispatchTable<RestTypes...>::dispatch(pos, sv, kb, tokens, contextFilters, filters);
    }
};

// Base case: no more strategies to try
template<>
struct CharacterParsingDispatchTable<> {
    static std::expected<qsizetype, json_query::json_path::Error> dispatch(
        qsizetype pos, QStringView sv, json_query::json_path::detail::KeyBuilder& kb, 
        QVector<json_query::json_path::Token>& tokens,
        QVector<json_query::json_path::ContextFilterFn>& contextFilters,
        QVector<json_query::json_path::FilterFn>& filters) {
        
        return std::unexpected(json_query::json_path::Error::UnsupportedFilter);
    }
};

// Compile-time dispatch table with prioritized strategy ordering
using CharacterParsingDispatcher = CharacterParsingDispatchTable<
    CharacterParsingType::DescendantSegment,
    CharacterParsingType::WildcardCharacter,
    CharacterParsingType::DotSegment,
    CharacterParsingType::BracketSegment,
    CharacterParsingType::BareSegment
>;

// ──────────────────────────────────────────────────────────────────────
//  compilePath - Refactored with TableGen-inspired architecture
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

    // TableGen-inspired monadic parsing loop with compile-time dispatch
    qsizetype pos = 1;
    while (pos < sv.size()) {
        // Use compile-time character parsing dispatch
        auto nextPosResult = CharacterParsingDispatcher::dispatch(pos, sv, kb, tokens, contextFilters, filters);
        
        if (!nextPosResult) {
            qCDebug(json_query::json_path::jsonPathLog) << "compilePath: parser returned error" << static_cast<int>(nextPosResult.error());
            return std::unexpected(nextPosResult.error());
        }
        
        pos = *nextPosResult;
    }

    qCDebug(json_query::json_path::jsonPathLog) << "compilePath: parsing completed successfully";
    return json_query::json_path::Compiled{ 
        std::move(tokens), 
        std::move(filters), 
        std::move(contextFilters) 
    };
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

// ──────────────────────────────────────────────────────────────────────
//  Modern Embedded Filter Compilation Implementation (Zero-Overhead)
// ──────────────────────────────────────────────────────────────────────

std::optional<Token> compileEmbeddedFilter(const QString& expr)
{
    qCDebug(jsonPathLog) << "compileEmbeddedFilter() expr=" << expr;
    
    // Try embedded filter parsing functions in priority order
    if (auto token = detail::parseEmbeddedCompare(expr)) return token;
    if (auto token = detail::parseEmbeddedExists(expr)) return token;
    if (auto token = detail::parseEmbeddedRegex(expr)) return token;
    if (auto token = detail::parseEmbeddedOr(expr)) return token;
    if (auto token = detail::parseEmbeddedAnd(expr)) return token;
    if (auto token = detail::parseEmbeddedIn(expr)) return token;
    if (auto token = detail::parseEmbeddedSelfCmp(expr)) return token;
    if (auto token = detail::parseEmbeddedNot(expr)) return token;
    
    qCDebug(jsonPathLog) << "compileEmbeddedFilter: no embedded parser matched";
    return std::nullopt;
}

std::optional<Token> compileEmbeddedContextFilter(const QString& expr)
{
    qCDebug(jsonPathLog) << "compileEmbeddedContextFilter() expr=" << expr;
    
    // Try embedded context filter parsing first
    if (auto token = parseEmbeddedAbsolutePathContext(expr)) return token;
    
    // Fall back to regular embedded filter parsing
    return compileEmbeddedFilter(expr);
}

std::optional<Token> parseEmbeddedAbsolutePathContext(QString s)
{
    qCDebug(jsonPathLog) << "parseEmbeddedAbsolutePathContext() s=" << s;
    
    // This will be implemented with embedded context filters
    // For now, return nullopt to maintain build compatibility
    return std::nullopt;
}

} // namespace json_query::json_path
