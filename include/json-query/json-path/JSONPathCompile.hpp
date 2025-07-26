#pragma once

// Initial scaffolding for the upcoming JSONPath parser split.
// In the next step we will move the full path-parsing logic from
// JSONPath.cpp into PathParser.cpp and expose it via the `parse` free
// function below.

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>
#include <QString>
#include <QStringView>
#include <functional>
#include <expected>
#include <optional>
#include <memory>

namespace json_query::json_path
{
    // ======================================================================
    //  Concept-based filter interfaces for zero-overhead evaluation
    // ======================================================================
    
    /**
     * @brief Concept for regular filter functions
     * 
     * A FilterConcept must provide a callable method that takes a QJsonValue
     * and returns a boolean indicating whether the value passes the filter.
     */
    template<typename T>
    concept FilterConcept = requires(T& filter, const QJsonValue& value) {
        { filter(value) } -> std::convertible_to<bool>;  // Must be callable with QJsonValue and return bool
    };

    /**
     * @brief Concept for context-aware filter functions
     * 
     * A ContextFilterConcept must provide a callable method that takes both
     * the current node and root document for context-aware filtering.
     */
    template<typename T>
    concept ContextFilterConcept = requires(T& filter, const QJsonValue& currentNode, const QJsonValue& rootDocument) {
        { filter(currentNode, rootDocument) } -> std::convertible_to<bool>;  // Must be callable with two QJsonValues
    };

    // Legacy type aliases for backward compatibility - these will use std::function
    // for storage in QVector containers where needed, but templates should prefer concepts
    // Note: Must use std::function for storage in QVector containers
    using FilterFn = std::function<bool (const QJsonValue&)>;

    // Context-aware filter function type for absolute path references
    // Receives both current node and root document context
    // Note: Must use std::function for storage in QVector containers
    using ContextFilterFn = std::function<bool (const QJsonValue& currentNode, const QJsonValue& rootDocument)>;

    enum class FunctionType { None, Length, Min, Max };

    // ======================================================================
    //  Template-based zero-overhead filter tokens (forward declarations)
    // ======================================================================
    
    // Forward declaration for use in Token
    class AnyFilterToken;
    
    /**
     * @brief Zero-overhead filter token that embeds filter logic directly
     * 
     * This template class eliminates the need for filter storage containers
     * by embedding the filter logic directly in the token. The filter is
     * stored with [[no_unique_address]] for zero memory overhead when possible.
     */
    template<FilterConcept Filter>
    class FilterToken {
    public:
        /**
         * @brief Construct a filter token with embedded filter logic
         * @param filter The filter function/lambda to embed
         */
        template<typename F>
        explicit FilterToken(F&& filter) noexcept
            : filter_(std::forward<F>(filter)) {}

        /**
         * @brief Evaluate the embedded filter directly (zero overhead)
         * @param value The JSON value to filter
         * @return true if the value passes the filter
         */
        [[nodiscard]] bool evaluate(const QJsonValue& value) const {
            return filter_(value);  // Direct call, no indirection
        }

        /**
         * @brief Check if this filter can handle the given value type
         * @param value The JSON value to check
         * @return true if the filter can process this value type
         */
        [[nodiscard]] bool canHandle(const QJsonValue& value) const noexcept {
            // Most filters can handle any JSON value type
            return true;
        }

    private:
        [[no_unique_address]] Filter filter_;  // Zero overhead with empty base optimization
    };

    /**
     * @brief Zero-overhead context-aware filter token
     * 
     * Similar to FilterToken but for context-aware filters that need both
     * the current node and root document for evaluation.
     */
    template<ContextFilterConcept ContextFilter>
    class ContextFilterToken {
    public:
        /**
         * @brief Construct a context filter token with embedded filter logic
         * @param filter The context filter function/lambda to embed
         */
        template<typename F>
        explicit ContextFilterToken(F&& filter) noexcept
            : filter_(std::forward<F>(filter)) {}

        /**
         * @brief Evaluate the embedded context filter directly (zero overhead)
         * @param currentNode The current JSON node being filtered
         * @param rootDocument The root document for context
         * @return true if the value passes the filter
         */
        [[nodiscard]] bool evaluate(const QJsonValue& currentNode, const QJsonValue& rootDocument) const {
            return filter_(currentNode, rootDocument);  // Direct call, no indirection
        }

        /**
         * @brief Check if this filter can handle the given value types
         * @param currentNode The current node to check
         * @param rootDocument The root document to check
         * @return true if the filter can process these value types
         */
        [[nodiscard]] bool canHandle(const QJsonValue& currentNode, const QJsonValue& rootDocument) const noexcept {
            return true;
        }

    private:
        [[no_unique_address]] ContextFilter filter_;  // Zero overhead with empty base optimization
    };

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
        
        // Legacy filter storage (for backward compatibility)
        std::size_t   filterId{};       // index into filter table
        std::size_t   contextFilterId{SIZE_MAX}; // index into context filter table (SIZE_MAX = not used)
        
        // Bracket group metadata for union vs sequential evaluation
        int           bracketGroupId{-1}; // -1 = not from bracket, >0 = bracket group ID
        
        bool isFromSameBracket(const Token& other) const {
            return bracketGroupId > 0 && bracketGroupId == other.bracketGroupId;
        }

        /**
         * @brief Check if this token uses legacy filter storage
         * @return true if using legacy index-based filter lookup
         */
        [[nodiscard]] bool hasLegacyFilter() const noexcept {
            return filterId < SIZE_MAX || contextFilterId < SIZE_MAX;
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

// ======================================================================
//  Template-based zero-overhead filter tokens
// ======================================================================

/**
 * @brief Type-erased filter token for storage compatibility
 * 
 * This provides a type-erased interface for cases where we need to store
 * different filter types in the same container. Uses concepts to ensure
 * type safety while providing runtime polymorphism when needed.
 * 
 * Uses shared_ptr for copyability to maintain Token's copy semantics.
 */
class AnyFilterToken {
public:
    /**
     * @brief Default constructor creates an empty filter token
     */
    AnyFilterToken() = default;

    /**
     * @brief Construct from any filter that satisfies FilterConcept
     * @param filter The filter to type-erase
     */
    template<FilterConcept Filter>
    explicit AnyFilterToken(Filter&& filter)
        : filter_(std::make_shared<FilterWrapper<std::decay_t<Filter>>>(std::forward<Filter>(filter))) {}

    /**
     * @brief Construct from any context filter that satisfies ContextFilterConcept
     * @param filter The context filter to type-erase
     */
    template<ContextFilterConcept Filter>
    explicit AnyFilterToken(Filter&& filter)
        : contextFilter_(std::make_shared<ContextFilterWrapper<std::decay_t<Filter>>>(std::forward<Filter>(filter))) {}

    /**
     * @brief Copy constructor (enabled by shared_ptr)
     */
    AnyFilterToken(const AnyFilterToken&) = default;

    /**
     * @brief Move constructor
     */
    AnyFilterToken(AnyFilterToken&&) noexcept = default;

    /**
     * @brief Copy assignment operator (enabled by shared_ptr)
     */
    AnyFilterToken& operator=(const AnyFilterToken&) = default;

    /**
     * @brief Move assignment operator
     */
    AnyFilterToken& operator=(AnyFilterToken&&) noexcept = default;

    /**
     * @brief Destructor
     */
    ~AnyFilterToken() = default;

    /**
     * @brief Evaluate the type-erased filter
     * @param value The JSON value to filter
     * @return true if the value passes the filter
     */
    [[nodiscard]] bool evaluate(const QJsonValue& value) const {
        return filter_ ? filter_->evaluate(value) : false;
    }

    /**
     * @brief Evaluate the type-erased context filter
     * @param currentNode The current JSON node
     * @param rootDocument The root document for context
     * @return true if the value passes the filter
     */
    [[nodiscard]] bool evaluateContext(const QJsonValue& currentNode, const QJsonValue& rootDocument) const {
        return contextFilter_ ? contextFilter_->evaluateContext(currentNode, rootDocument) : false;
    }

    /**
     * @brief Check if this is a regular filter
     * @return true if this contains a regular filter
     */
    [[nodiscard]] bool isRegularFilter() const noexcept {
        return filter_ != nullptr;
    }

    /**
     * @brief Check if this is a context filter
     * @return true if this contains a context filter
     */
    [[nodiscard]] bool isContextFilter() const noexcept {
        return contextFilter_ != nullptr;
    }

    /**
     * @brief Check if this token contains any filter
     * @return true if this contains either a regular or context filter
     */
    [[nodiscard]] bool hasFilter() const noexcept {
        return isRegularFilter() || isContextFilter();
    }

private:
    struct FilterBase {
        virtual ~FilterBase() = default;
        virtual bool evaluate(const QJsonValue& value) const = 0;
    };

    struct ContextFilterBase {
        virtual ~ContextFilterBase() = default;
        virtual bool evaluateContext(const QJsonValue& currentNode, const QJsonValue& rootDocument) const = 0;
    };

    template<FilterConcept Filter>
    struct FilterWrapper : FilterBase {
        explicit FilterWrapper(Filter&& f) : filter_(std::forward<Filter>(f)) {}
        bool evaluate(const QJsonValue& value) const override {
            return filter_(value);
        }
    private:
        Filter filter_;
    };

    template<ContextFilterConcept Filter>
    struct ContextFilterWrapper : ContextFilterBase {
        explicit ContextFilterWrapper(Filter&& f) : filter_(std::forward<Filter>(f)) {}
        bool evaluateContext(const QJsonValue& currentNode, const QJsonValue& rootDocument) const override {
            return filter_(currentNode, rootDocument);
        }
    private:
        Filter filter_;
    };

    std::shared_ptr<FilterBase> filter_;
    std::shared_ptr<ContextFilterBase> contextFilter_;
};

} // namespace json_query::json_path
