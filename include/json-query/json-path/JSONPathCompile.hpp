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
#include <variant>
#include <vector>

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
template <typename T>
concept FilterConcept = requires(T t, const QJsonValue& value) {
    { t(value) } -> std::convertible_to<bool>;
};

/**
 * @brief Concept for context-aware filter functions
 *
 * A ContextFilterConcept must provide a callable method that takes both
 * the current node and root document for context-aware filtering.
 */
template <typename T>
concept ContextFilterConcept = requires(T t, const QJsonValue& currentNode, const QJsonValue& rootDocument) {
    { t(currentNode, rootDocument) } -> std::convertible_to<bool>;
};

// TODO: Remove these legacy type aliases after migrating all usages to embedded filters
// These are temporarily kept to maintain build compatibility during refactoring
using FilterFn        = std::function<bool(const QJsonValue&)>;
using ContextFilterFn = std::function<bool(const QJsonValue& currentNode, const QJsonValue& rootDocument)>;

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
template <FilterConcept Filter>
class FilterToken
{
  public:
    /**
     * @brief Construct a filter token with embedded filter logic
     * @param filter The filter function/lambda to embed
     */
    template <typename F>
    explicit FilterToken(F&& filter) noexcept : filter_(std::forward<F>(filter))
    {
    }

    /**
     * @brief Evaluate the embedded filter directly (zero overhead)
     * @param value The JSON value to filter
     * @return true if the value passes the filter
     */
    [[nodiscard]] bool evaluate(const QJsonValue& value) const
    {
        return filter_(value); // Direct call, no indirection
    }

    /**
     * @brief Check if this filter can handle the given value type
     * @param value The JSON value to check
     * @return true if the filter can process this value type
     */
    [[nodiscard]] bool canHandle(const QJsonValue& value) const noexcept
    {
        // Most filters can handle any JSON value type
        return true;
    }

  private:
    [[no_unique_address]] Filter filter_; // Zero overhead with empty base optimization
};

/**
 * @brief Zero-overhead context-aware filter token
 *
 * Similar to FilterToken but for context-aware filters that need both
 * the current node and root document for evaluation.
 */
template <ContextFilterConcept ContextFilter>
class ContextFilterToken
{
  public:
    /**
     * @brief Construct a context filter token with embedded filter logic
     * @param filter The context filter function/lambda to embed
     */
    template <typename F>
    explicit ContextFilterToken(F&& filter) noexcept : filter_(std::forward<F>(filter))
    {
    }

    /**
     * @brief Evaluate the embedded context filter directly (zero overhead)
     * @param currentNode The current JSON node being filtered
     * @param rootDocument The root document for context
     * @return true if the value passes the filter
     */
    [[nodiscard]] bool evaluate(const QJsonValue& currentNode, const QJsonValue& rootDocument) const
    {
        return filter_(currentNode, rootDocument); // Direct call, no indirection
    }

    /**
     * @brief Check if this filter can handle the given value types
     * @param currentNode The current node to check
     * @param rootDocument The root document to check
     * @return true if the filter can process these value types
     */
    [[nodiscard]] bool canHandle(const QJsonValue& currentNode, const QJsonValue& rootDocument) const noexcept
    {
        return true;
    }

  private:
    [[no_unique_address]] ContextFilter filter_; // Zero overhead with empty base optimization
};

enum class FunctionType
{
    None,
    Length,
    Min,
    Max
};

// ======================================================================
//  Zero-Overhead Copyable Filter Embedding System
// ======================================================================

/**
 * @brief Small buffer optimization for filter storage
 *
 * Uses a fixed-size buffer to store small filters inline without heap allocation.
 * Larger filters fall back to shared_ptr for copyability.
 */
template <std::size_t BufferSize = 32>
class CompactFilterStorage
{
  public:
    /**
     * @brief Default constructor creates empty storage
     */
    CompactFilterStorage() = default;

    /**
     * @brief Construct with a filter that fits in the small buffer
     * @param filter The filter to store inline
     */
    template <FilterConcept Filter>
    explicit CompactFilterStorage(Filter&& filter)
        requires(sizeof(std::decay_t<Filter>) <= BufferSize && std::is_trivially_copyable_v<std::decay_t<Filter>>)
    {
        using FilterType = std::decay_t<Filter>;
        static_assert(sizeof(FilterType) <= BufferSize, "Filter too large for inline storage");

        InlineStorage storage;
        new (storage.buffer.data()) FilterType(std::forward<Filter>(filter));
        storage.evaluator = [](const void* data, const QJsonValue& value) -> bool
        {
            const auto* filter = static_cast<const FilterType*>(data);
            return (*filter)(value);
        };

        storage_ = std::move(storage);
    }

    /**
     * @brief Construct with a large filter using shared storage
     * @param filter The filter to store in shared memory
     */
    template <FilterConcept Filter>
    explicit CompactFilterStorage(Filter&& filter)
        requires(sizeof(std::decay_t<Filter>) > BufferSize || !std::is_trivially_copyable_v<std::decay_t<Filter>>)
    {
        using FilterType = std::decay_t<Filter>;

        HeapStorage storage;
        storage.sharedFilter = std::make_shared<FilterType>(std::forward<Filter>(filter));
        storage.evaluator    = [](const std::shared_ptr<void>& ptr, const QJsonValue& value) -> bool
        {
            const auto* filter = static_cast<const FilterType*>(ptr.get());
            return (*filter)(value);
        };

        storage_ = std::move(storage);
    }

    /**
     * @brief Copy constructor (automatic with variant)
     */
    CompactFilterStorage(const CompactFilterStorage&) = default;

    /**
     * @brief Move constructor (automatic with variant)
     */
    CompactFilterStorage(CompactFilterStorage&&) noexcept = default;

    /**
     * @brief Copy assignment operator (automatic with variant)
     */
    CompactFilterStorage& operator=(const CompactFilterStorage&) = default;

    /**
     * @brief Move assignment operator (automatic with variant)
     */
    CompactFilterStorage& operator=(CompactFilterStorage&&) noexcept = default;

    /**
     * @brief Destructor (automatic with variant)
     */
    ~CompactFilterStorage() = default;

    /**
     * @brief Evaluate the stored filter
     * @param value The JSON value to filter
     * @return true if the value passes the filter
     */
    [[nodiscard]] bool evaluate(const QJsonValue& value) const
    {
        return std::visit(
            [&](const auto& storage) -> bool
            {
                using StorageType = std::decay_t<decltype(storage)>;

                if constexpr (std::is_same_v<StorageType, EmptyStorage>)
                    return false;
                else if constexpr (std::is_same_v<StorageType, InlineStorage>)
                    return storage.evaluator(storage.buffer.data(), value);
                else if constexpr (std::is_same_v<StorageType, HeapStorage>)
                    return storage.evaluator(storage.sharedFilter, value);
                return false;
            },
            storage_);
    }

    /**
     * @brief Check if this storage contains a filter
     * @return true if a filter is stored
     */
    [[nodiscard]] bool hasFilter() const noexcept { return !std::holds_alternative<EmptyStorage>(storage_); }

    /**
     * @brief Check if the filter is stored inline (zero heap allocation)
     * @return true if using inline storage
     */
    [[nodiscard]] bool isInlineStorage() const noexcept { return std::holds_alternative<InlineStorage>(storage_); }

  private:
    // Empty state
    struct EmptyStorage
    {
    };

    // Inline storage for small filters
    struct InlineStorage
    {
        std::array<std::byte, BufferSize> buffer;
        bool (*evaluator)(const void*, const QJsonValue&) = nullptr;
    };

    // Heap storage for large filters
    struct HeapStorage
    {
        std::shared_ptr<void> sharedFilter;
        bool (*evaluator)(const std::shared_ptr<void>&, const QJsonValue&) = nullptr;
    };

    std::variant<EmptyStorage, InlineStorage, HeapStorage> storage_;
};

/**
 * @brief Context filter storage with small buffer optimization
 */
template <std::size_t BufferSize = 32>
class CompactContextFilterStorage
{
  public:
    /**
     * @brief Default constructor creates empty storage
     */
    CompactContextFilterStorage() = default;

    /**
     * @brief Construct with a context filter that fits in the small buffer
     */
    template <ContextFilterConcept Filter>
    explicit CompactContextFilterStorage(Filter&& filter)
        requires(sizeof(std::decay_t<Filter>) <= BufferSize && std::is_trivially_copyable_v<std::decay_t<Filter>>)
    {
        using FilterType = std::decay_t<Filter>;
        static_assert(sizeof(FilterType) <= BufferSize, "Context filter too large for inline storage");

        InlineStorage storage;
        new (storage.buffer.data()) FilterType(std::forward<Filter>(filter));
        storage.evaluator = [](const void* data, const QJsonValue& currentNode, const QJsonValue& rootDocument) -> bool
        {
            const auto* filter = static_cast<const FilterType*>(data);
            return (*filter)(currentNode, rootDocument);
        };

        storage_ = std::move(storage);
    }

    /**
     * @brief Construct with a large context filter using shared storage
     */
    template <ContextFilterConcept Filter>
    explicit CompactContextFilterStorage(Filter&& filter)
        requires(sizeof(std::decay_t<Filter>) > BufferSize || !std::is_trivially_copyable_v<std::decay_t<Filter>>)
    {
        using FilterType = std::decay_t<Filter>;

        HeapStorage storage;
        storage.sharedFilter = std::make_shared<FilterType>(std::forward<Filter>(filter));
        storage.evaluator =
            [](const std::shared_ptr<void>& ptr, const QJsonValue& currentNode, const QJsonValue& rootDocument) -> bool
        {
            const auto* filter = static_cast<const FilterType*>(ptr.get());
            return (*filter)(currentNode, rootDocument);
        };

        storage_ = std::move(storage);
    }

    /**
     * @brief Copy constructor (automatic with variant)
     */
    CompactContextFilterStorage(const CompactContextFilterStorage&) = default;

    /**
     * @brief Move constructor (automatic with variant)
     */
    CompactContextFilterStorage(CompactContextFilterStorage&&) noexcept = default;

    /**
     * @brief Copy assignment operator (automatic with variant)
     */
    CompactContextFilterStorage& operator=(const CompactContextFilterStorage&) = default;

    /**
     * @brief Move assignment operator (automatic with variant)
     */
    CompactContextFilterStorage& operator=(CompactContextFilterStorage&&) noexcept = default;

    /**
     * @brief Destructor (automatic with variant)
     */
    ~CompactContextFilterStorage() = default;

    /**
     * @brief Evaluate the stored context filter
     * @param currentNode The current JSON node
     * @param rootDocument The root document for context
     * @return true if the value passes the filter
     */
    [[nodiscard]] bool evaluateContext(const QJsonValue& currentNode, const QJsonValue& rootDocument) const
    {
        return std::visit(
            [&](const auto& storage) -> bool
            {
                using StorageType = std::decay_t<decltype(storage)>;

                if constexpr (std::is_same_v<StorageType, EmptyStorage>)
                    return false;
                else if constexpr (std::is_same_v<StorageType, InlineStorage>)
                    return storage.evaluator(storage.buffer.data(), currentNode, rootDocument);
                else if constexpr (std::is_same_v<StorageType, HeapStorage>)
                    return storage.evaluator(storage.sharedFilter, currentNode, rootDocument);
                return false;
            },
            storage_);
    }

    /**
     * @brief Check if this storage contains a context filter
     * @return true if a context filter is stored
     */
    [[nodiscard]] bool hasFilter() const noexcept { return !std::holds_alternative<EmptyStorage>(storage_); }

    /**
     * @brief Check if the context filter is stored inline (zero heap allocation)
     * @return true if using inline storage
     */
    [[nodiscard]] bool isInlineStorage() const noexcept { return std::holds_alternative<InlineStorage>(storage_); }

  private:
    // Empty state
    struct EmptyStorage
    {
    };

    // Inline storage for small filters
    struct InlineStorage
    {
        std::array<std::byte, BufferSize> buffer;
        bool (*evaluator)(const void*, const QJsonValue&, const QJsonValue&) = nullptr;
    };

    // Heap storage for large filters
    struct HeapStorage
    {
        std::shared_ptr<void> sharedFilter;
        bool (*evaluator)(const std::shared_ptr<void>&, const QJsonValue&, const QJsonValue&) = nullptr;
    };

    std::variant<EmptyStorage, InlineStorage, HeapStorage> storage_;
};

/**
 * @brief Zero-overhead embedded filter for Token struct
 *
 * Combines regular and context filters in a single, copyable structure
 * with small buffer optimization for maximum performance.
 */
class EmbeddedFilter
{
  public:
    /**
     * @brief Default constructor creates empty filter
     */
    EmbeddedFilter() = default;

    /**
     * @brief Construct with a regular filter
     */
    template <FilterConcept Filter>
    explicit EmbeddedFilter(Filter&& filter) : regularFilter_(std::forward<Filter>(filter))
    {
    }

    /**
     * @brief Construct with a context filter
     */
    template <ContextFilterConcept Filter>
    explicit EmbeddedFilter(Filter&& filter) : contextFilter_(std::forward<Filter>(filter))
    {
    }

    /**
     * @brief Copy constructor (fully copyable)
     */
    EmbeddedFilter(const EmbeddedFilter&) = default;

    /**
     * @brief Move constructor
     */
    EmbeddedFilter(EmbeddedFilter&&) noexcept = default;

    /**
     * @brief Copy assignment operator
     */
    EmbeddedFilter& operator=(const EmbeddedFilter&) = default;

    /**
     * @brief Move assignment operator
     */
    EmbeddedFilter& operator=(EmbeddedFilter&&) noexcept = default;

    /**
     * @brief Destructor
     */
    ~EmbeddedFilter() = default;

    /**
     * @brief Evaluate regular filter
     * @param value The JSON value to filter
     * @return true if the value passes the filter
     */
    [[nodiscard]] bool evaluate(const QJsonValue& value) const { return regularFilter_.evaluate(value); }

    /**
     * @brief Evaluate context filter
     * @param currentNode The current JSON node
     * @param rootDocument The root document for context
     * @return true if the value passes the filter
     */
    [[nodiscard]] bool evaluateContext(const QJsonValue& currentNode, const QJsonValue& rootDocument) const
    {
        return contextFilter_.evaluateContext(currentNode, rootDocument);
    }

    /**
     * @brief Check if this contains a regular filter
     * @return true if a regular filter is embedded
     */
    [[nodiscard]] bool hasRegularFilter() const noexcept { return regularFilter_.hasFilter(); }

    /**
     * @brief Check if this contains a context filter
     * @return true if a context filter is embedded
     */
    [[nodiscard]] bool hasContextFilter() const noexcept { return contextFilter_.hasFilter(); }

    /**
     * @brief Check if this contains any filter
     * @return true if any filter is embedded
     */
    [[nodiscard]] bool hasFilter() const noexcept { return hasRegularFilter() || hasContextFilter(); }

    /**
     * @brief Check if filters are stored inline (zero heap allocation)
     * @return true if using only inline storage
     */
    [[nodiscard]] bool isZeroOverhead() const noexcept
    {
        return (!hasRegularFilter() || regularFilter_.isInlineStorage()) &&
               (!hasContextFilter() || contextFilter_.isInlineStorage());
    }

  private:
    CompactFilterStorage<32>        regularFilter_;
    CompactContextFilterStorage<32> contextFilter_;
};

// ======================================================================
//  Compact, pre-decoded token layout
// ======================================================================
struct Slice
{
    qsizetype start{}, end{}, step{};
};

struct Token
{
    enum class Kind : quint8
    {
        Key,
        KeyList,
        Index,
        Slice,
        Wildcard,
        Recursive,
        Filter
    };
    Kind      kind{Kind::Key};
    qsizetype index{}; // for Kind::Index
    Slice     slice{}; // for Kind::Slice
    quint32   hash{};  // cached object-key hash
    QString   key{};   // for Kind::Key / KeyList (joined by '\n') / Filter

    // Zero-overhead embedded filter (new architecture)
    std::optional<EmbeddedFilter> embeddedFilter{};

    // Bracket group metadata for union vs sequential evaluation
    int bracketGroupId{-1}; // -1 = not from bracket, >0 = bracket group ID

    bool isFromSameBracket(const Token& other) const
    {
        return bracketGroupId > 0 && bracketGroupId == other.bracketGroupId;
    }

    /**
     * @brief Check if this token has an embedded filter
     * @return true if using new embedded filter architecture
     */
    [[nodiscard]] bool hasEmbeddedFilter() const noexcept
    {
        return embeddedFilter.has_value() && embeddedFilter->hasFilter();
    }

    /**
     * @brief Check if this token uses legacy filter storage
     * @return true if using legacy index-based filter lookup
     */
    [[nodiscard]] bool hasLegacyFilter() const noexcept { return filterId < SIZE_MAX || contextFilterId < SIZE_MAX; }

    // Legacy filter storage (for backward compatibility during migration)
    std::size_t filterId{};                // index into filter table
    std::size_t contextFilterId{SIZE_MAX}; // index into context filter table (SIZE_MAX = not used)

    /**
     * @brief Embed a regular filter directly in this token
     * @param filter The filter to embed
     */
    template <FilterConcept Filter>
    void embedFilter(Filter&& filter)
    {
        embeddedFilter = EmbeddedFilter(std::forward<Filter>(filter));
    }

    /**
     * @brief Embed a context filter directly in this token
     * @param filter The context filter to embed
     */
    template <ContextFilterConcept Filter>
    void embedContextFilter(Filter&& filter)
    {
        embeddedFilter = EmbeddedFilter(std::forward<Filter>(filter));
    }

    /**
     * @brief Evaluate the embedded filter (if present)
     * @param value The JSON value to filter
     * @return true if the value passes the filter, false if no filter
     */
    [[nodiscard]] bool evaluateEmbeddedFilter(const QJsonValue& value) const
    {
        return embeddedFilter ? embeddedFilter->evaluate(value) : false;
    }

    /**
     * @brief Evaluate the embedded context filter (if present)
     * @param currentNode The current JSON node
     * @param rootDocument The root document for context
     * @return true if the value passes the filter, false if no filter
     */
    [[nodiscard]] bool evaluateEmbeddedContextFilter(const QJsonValue& currentNode,
                                                     const QJsonValue& rootDocument) const
    {
        return embeddedFilter ? embeddedFilter->evaluateContext(currentNode, rootDocument) : false;
    }
};

// ------------------------------------------------------------------
//  Parser / compiler error codes
// ------------------------------------------------------------------
enum class Error : std::uint8_t
{
    Ok = 0, // not used in expected<T,E>
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
    InvalidIdentifier, // RFC 9535: invalid member-name-shorthand
    UnexpectedAfterRoot
};

[[nodiscard]] inline constexpr std::string_view toString(Error e) noexcept
{
    using enum Error;
    switch (e)
    {
    case MissingRoot:
        return "JSONPath must start with root identifier '$' or '@'";
    case TrailingDot:
        return "trailing '.' in segment";
    case TrailingRecursive:
        return "trailing '..' in descendant segment";
    case EmptySegment:
        return "empty segment";
    case BlankInKey:
        return "blank in member name";
    case UnmatchedBracket:
        return "unmatched '[' in selector";
    case UnmatchedQuote:
        return "unmatched quote in selector";
    case UnsupportedFilter:
        return "unsupported filter-selector expression";
    case InvalidSlice:
        return "invalid slice-selector syntax";
    case InvalidIndex:
        return "invalid index-selector syntax";
    case InvalidIdentifier:
        return "invalid member name identifier";
    case UnexpectedAfterRoot:
        return "root identifier must be followed by '.' or '['";
    default:
        return "unknown compilation error";
    }
}

// ======================================================================
//  Compilation API - extracted from JSONPath for testability
// ======================================================================

// Compiled result structure containing tokens and filters
struct Compiled
{
    std::vector<Token>           tokens;
    std::vector<FilterFn>        filters;
    std::vector<ContextFilterFn> contextFilters;
};

// ──────────────────────────────────────────────────────────────────────
//  Core compilation functions
// ──────────────────────────────────────────────────────────────────────

/// Compile a JSONPath string into tokens and filters
/// @param path The JSONPath string to compile (without trailing functions)
/// @return Compiled tokens and filters, or compilation error
[[nodiscard]] std::expected<Compiled, Error> compilePath(QStringView path);

/// Detect and remove trailing function from path (.length(), .min(), .max())
/// @param path Path string that will be modified to remove trailing function
/// @return The detected function type
[[nodiscard]] FunctionType detectTrailingFunction(QString& path);

/// Compile a filter expression into a Token with associated filter
/// @param expr The filter expression string
/// @param filters Output vector to store the compiled filter function (legacy)
/// @return Token representing the filter, or nullopt if compilation failed
[[nodiscard]] std::optional<Token> compileFilter(const QString& expr, std::vector<FilterFn>& filters);

/// Context-aware filter compilation for absolute path references
/// @param expr The filter expression string
/// @param contextFilters Output vector to store the compiled context filter function (legacy)
/// @param filters Output vector to store the compiled filter function (legacy)
/// @return Token representing the filter, or nullopt if compilation failed
[[nodiscard]] std::optional<Token> compileContextFilter(const QString&                expr,
                                                        std::vector<ContextFilterFn>& contextFilters,
                                                        std::vector<FilterFn>&        filters);

/// Parse absolute path references in filter expressions (context-aware)
/// @param s The absolute path reference string
/// @param out Output vector to store the compiled context filter function (legacy)
/// @return Token representing the filter, or nullopt if compilation failed
[[nodiscard]] std::optional<Token> parseAbsolutePathContext(const QString& s, std::vector<ContextFilterFn>& out);

// ──────────────────────────────────────────────────────────────────────
//  Modern Embedded Filter Compilation API (Zero-Overhead)
// ──────────────────────────────────────────────────────────────────────

/// Compile a filter expression into a Token with embedded filter (modern API)
/// @param expr The filter expression string
/// @return Token with embedded filter, or nullopt if compilation failed
[[nodiscard]] std::optional<Token> compileEmbeddedFilter(const QString& expr);

/// Context-aware filter compilation with embedded filter (modern API)
/// @param expr The filter expression string
/// @return Token with embedded context filter, or nullopt if compilation failed
[[nodiscard]] std::optional<Token> compileEmbeddedContextFilter(const QString& expr);

/// Parse absolute path references with embedded context filter (modern API)
/// @param s The absolute path reference string
/// @return Token with embedded context filter, or nullopt if compilation failed
[[nodiscard]] std::optional<Token> parseEmbeddedAbsolutePathContext(QString s);

// ──────────────────────────────────────────────────────────────────────
//  Convenience API
// ──────────────────────────────────────────────────────────────────────

/// High-level compilation function that handles trailing functions
/// @param rawPath The complete JSONPath string including any trailing functions
/// @return Compiled result with function type, tokens, and filters
struct CompilationResult
{
    FunctionType function;
    Compiled     compiled;
};

[[nodiscard]] std::expected<CompilationResult, Error> compile(QStringView rawPath);

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
class AnyFilterToken
{
  public:
    /**
     * @brief Default constructor creates an empty filter token
     */
    AnyFilterToken() = default;

    /**
     * @brief Construct from any filter that satisfies FilterConcept
     * @param filter The filter to type-erase
     */
    template <FilterConcept Filter>
    explicit AnyFilterToken(Filter&& filter)
        : filter_(std::make_shared<FilterWrapper<std::decay_t<Filter>>>(std::forward<Filter>(filter)))
    {
    }

    /**
     * @brief Construct from any context filter that satisfies ContextFilterConcept
     * @param filter The context filter to type-erase
     */
    template <ContextFilterConcept Filter>
    explicit AnyFilterToken(Filter&& filter)
        : contextFilter_(std::make_shared<ContextFilterWrapper<std::decay_t<Filter>>>(std::forward<Filter>(filter)))
    {
    }

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
    [[nodiscard]] bool evaluate(const QJsonValue& value) const { return filter_ ? filter_->evaluate(value) : false; }

    /**
     * @brief Evaluate the type-erased context filter
     * @param currentNode The current JSON node
     * @param rootDocument The root document for context
     * @return true if the value passes the filter
     */
    [[nodiscard]] bool evaluateContext(const QJsonValue& currentNode, const QJsonValue& rootDocument) const
    {
        return contextFilter_ ? contextFilter_->evaluateContext(currentNode, rootDocument) : false;
    }

    /**
     * @brief Check if this is a regular filter
     * @return true if this contains a regular filter
     */
    [[nodiscard]] bool isRegularFilter() const noexcept { return filter_ != nullptr; }

    /**
     * @brief Check if this is a context filter
     * @return true if this contains a context filter
     */
    [[nodiscard]] bool isContextFilter() const noexcept { return contextFilter_ != nullptr; }

    /**
     * @brief Check if this token contains any filter
     * @return true if this contains either a regular or context filter
     */
    [[nodiscard]] bool hasFilter() const noexcept { return isRegularFilter() || isContextFilter(); }

  private:
    struct FilterBase
    {
        virtual ~FilterBase()                                = default;
        virtual bool evaluate(const QJsonValue& value) const = 0;
    };

    struct ContextFilterBase
    {
        virtual ~ContextFilterBase()                                                                      = default;
        virtual bool evaluateContext(const QJsonValue& currentNode, const QJsonValue& rootDocument) const = 0;
    };

    template <FilterConcept Filter>
    struct FilterWrapper : FilterBase
    {
        explicit FilterWrapper(Filter&& f) : filter_(std::forward<Filter>(f)) {}
        bool evaluate(const QJsonValue& value) const override { return filter_(value); }

      private:
        Filter filter_;
    };

    template <ContextFilterConcept Filter>
    struct ContextFilterWrapper : ContextFilterBase
    {
        explicit ContextFilterWrapper(Filter&& f) : filter_(std::forward<Filter>(f)) {}
        bool evaluateContext(const QJsonValue& currentNode, const QJsonValue& rootDocument) const override
        {
            return filter_(currentNode, rootDocument);
        }

      private:
        Filter filter_;
    };

    std::shared_ptr<FilterBase>        filter_;
    std::shared_ptr<ContextFilterBase> contextFilter_;
};

} // namespace json_query::json_path
