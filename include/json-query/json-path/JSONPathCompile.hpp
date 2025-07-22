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
        case MissingRoot      : return "path must start with '$' or '@'";
        case TrailingDot      : return "trailing '.'";
        case TrailingRecursive: return "trailing '..'";
        case EmptySegment     : return "empty property segment";
        case BlankInKey       : return "blank in property name";
        case UnmatchedBracket : return "unmatched '['";
        case UnmatchedQuote   : return "unmatched quote inside [...]";
        case UnsupportedFilter: return "unsupported filter expression";
        case InvalidSlice     : return "bad array slice";
        case InvalidIndex     : return "bad numeric index";
        case InvalidIdentifier: return "invalid identifier";
        case UnexpectedAfterRoot: return "root must be followed by '.' or '['";
        default               : return "unknown error";
    }
}

// ======================================================================
//  Compilation API - extracted from JSONPath for testability
// ======================================================================

// Compiled result structure containing tokens and filters
struct Compiled { 
    QVector<Token> tokens; 
    QVector<FilterFn> filters; 
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
