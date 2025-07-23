#pragma once

// Initial scaffolding for the upcoming JSONPath parser split.
// In the next step we will move the full path-parsing logic from
// JSONPath.cpp into PathParser.cpp and expose it via the `parse` free
// function below.

#include <QVector>
#include <QString>
#include <QJsonValue>
#include <variant>
#include <tuple>
#include <functional>
#include <expected>
#include <optional>

namespace json_query::json_path
{
    // Filter function type - moved here for better organization
    using FilterFn = std::function<bool (const QJsonValue&)>;

    // Context-aware filter function type for absolute path references
    // Receives both current node and root document context
    using ContextFilterFn = std::function<bool (const QJsonValue& currentNode, const QJsonValue& rootDocument)>;

    enum class FunctionType { None, Length, Min, Max };

    // ======================================================================
    //  Compact, pre-decoded token layout
    // ======================================================================
    struct Slice { qsizetype start{}, end{}, step{}; };

    struct Token {
        enum class Kind : quint8 {
            Key, KeyList, Index, Slice, Wildcard, Recursive, Filter
        };
        Kind          kind {Kind::Key};
        qsizetype     index{};          // for Kind::Index
        Slice         slice{};          // for Kind::Slice
        quint32       hash{};           // cached object-key hash
        QString       key{};            // for Kind::Key / KeyList (joined by '\n') / Filter
        std::size_t   filterId{};       // index into filter table
        std::size_t   contextFilterId{SIZE_MAX}; // index into context filter table (SIZE_MAX = not used)
        
        // Bracket group metadata for union vs sequential evaluation
        int           bracketGroupId{-1}; // -1 = not from bracket, >0 = bracket group ID
        
        bool isFromSameBracket(const Token& other) const {
            return bracketGroupId > 0 && bracketGroupId == other.bracketGroupId;
        }
    };   

// ------------------------------------------------------------------
//  Parser / compiler error codes
// ------------------------------------------------------------------
enum class Error : std::uint8_t {
    Ok = 0,               // not used in expected<T,E>
    MissingRoot,
    TrailingDot,
    TrailingRecursive,
    EmptySegment,
    BlankInKey,
    UnmatchedBracket,
    UnmatchedQuote,
    UnsupportedFilter,
    InvalidSlice,
    InvalidIndex,
    InvalidIdentifier,    // RFC 9535: invalid member-name-shorthand
    UnexpectedAfterRoot
};

[[nodiscard]] inline constexpr std::string_view
toString(Error e) noexcept
{
    using enum Error;
    switch (e)
    {
        case MissingRoot      : return "JSONPath must start with root identifier '$' or '@'";
        case TrailingDot      : return "trailing '.' in segment";
        case TrailingRecursive: return "trailing '..' in descendant segment";
        case EmptySegment     : return "empty segment";
        case BlankInKey       : return "blank in member name";
        case UnmatchedBracket : return "unmatched '[' in selector";
        case UnmatchedQuote   : return "unmatched quote in selector";
        case UnsupportedFilter: return "unsupported filter-selector expression";
        case InvalidSlice     : return "invalid slice-selector syntax";
        case InvalidIndex     : return "invalid index-selector syntax";
        case InvalidIdentifier: return "invalid member name identifier";
        case UnexpectedAfterRoot: return "root identifier must be followed by '.' or '['";
        default               : return "unknown compilation error";
    }
}

// ======================================================================
//  Compilation API - extracted from JSONPath for testability
// ======================================================================

// Compiled result structure containing tokens and filters
struct Compiled { 
    QVector<Token> tokens; 
    QVector<FilterFn> filters; 
    QVector<ContextFilterFn> contextFilters; 
};

// ──────────────────────────────────────────────────────────────────────
//  Core compilation functions
// ──────────────────────────────────────────────────────────────────────

/// Compile a JSONPath string into tokens and filters
/// @param path The JSONPath string to compile (without trailing functions)
/// @return Compiled tokens and filters, or compilation error
[[nodiscard]] std::expected<Compiled, Error> 
compilePath(QStringView path);

/// Detect and remove trailing function from path (.length(), .min(), .max())
/// @param path Path string that will be modified to remove trailing function
/// @return The detected function type
[[nodiscard]] FunctionType 
detectTrailingFunction(QString& path);

/// Compile a filter expression into a Token with associated FilterFn
/// @param expr The filter expression string
/// @param filters Output vector to store the compiled filter function
/// @return Token representing the filter, or nullopt if compilation failed
[[nodiscard]] std::optional<Token> 
compileFilter(const QString& expr, QVector<FilterFn>& filters);

/// Context-aware filter compilation for absolute path references
[[nodiscard]] std::optional<Token> 
compileContextFilter(const QString& expr, QVector<ContextFilterFn>& contextFilters, QVector<FilterFn>& filters);

/// Parse absolute path references in filter expressions (context-aware)
[[nodiscard]] std::optional<Token>
parseAbsolutePathContext(QString s, QVector<ContextFilterFn>& out);

// ──────────────────────────────────────────────────────────────────────
//  Convenience API
// ──────────────────────────────────────────────────────────────────────

/// High-level compilation function that handles trailing functions
/// @param rawPath The complete JSONPath string including any trailing functions
/// @return Compiled result with function type, tokens, and filters
struct CompilationResult {
    FunctionType function;
    Compiled compiled;
};

[[nodiscard]] std::expected<CompilationResult, Error>
compile(QStringView rawPath);

} // namespace json_query::json_path
