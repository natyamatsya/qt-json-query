#include "json-query/json-path/JSONPathEvalHelpers.hpp"
#include "json-query/json-path/JSONPathTokenEvaluators.hpp"
#include "json-query/json-path/JSONPathTokenDispatch.hpp"
#include "json-query/json-path/internal/ContextAwareContainerCursor.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/JSONPathLog.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>
#include <QSet>
#include <QDebug>
#include <algorithm>
#include <limits>

namespace json_query::json_path::detail {

using json_query::json_path::internal::ContainerCursor;
using internal::acquirePooledArray;

// ---------------------------------------------------------------------------
//  Basic evaluation helpers
// ---------------------------------------------------------------------------

int normalizeIndex(int idx, int size)
{
    return idx < 0 ? size + idx : idx;
}

// ---------------------------------------------------------------------------
//  TableGen-Inspired evalSlice Architecture
// ---------------------------------------------------------------------------

/**
 * @brief Slice processing strategy types for compile-time dispatch
 * 
 * Each type represents a distinct slice processing pattern with specific
 * characteristics and optimization opportunities.
 */
enum class SliceProcessingType {
    ZeroStep,        // step == 0 (empty result, RFC 9535 compliance)
    ForwardSlice,    // step > 0 (forward iteration)
    BackwardSlice,   // step < 0 (backward iteration)
    DefaultSlice     // fallback for edge cases
};

/**
 * @brief TableGen-style slice processing pattern definitions
 * 
 * Each specialization defines the characteristics and matching logic
 * for a specific slice processing strategy.
 */
template<SliceProcessingType Type>
struct SliceProcessingDef {
    static constexpr bool enabled = false;
    static constexpr int priority = 0;
    
    static bool matches(const Slice& s, int len) { return false; }
};

// Specialization for zero step handling
template<>
struct SliceProcessingDef<SliceProcessingType::ZeroStep> {
    static constexpr bool enabled = true;
    static constexpr int priority = 1000;  // Highest priority
    
    static bool matches(const Slice& s, int len) {
        return s.step == 0;
    }
};

// Specialization for forward slice processing
template<>
struct SliceProcessingDef<SliceProcessingType::ForwardSlice> {
    static constexpr bool enabled = true;
    static constexpr int priority = 800;
    
    static bool matches(const Slice& s, int len) {
        return s.step > 0;
    }
};

// Specialization for backward slice processing
template<>
struct SliceProcessingDef<SliceProcessingType::BackwardSlice> {
    static constexpr bool enabled = true;
    static constexpr int priority = 700;
    
    static bool matches(const Slice& s, int len) {
        return s.step < 0;
    }
};

// Specialization for default slice processing
template<>
struct SliceProcessingDef<SliceProcessingType::DefaultSlice> {
    static constexpr bool enabled = true;
    static constexpr int priority = 100;  // Lowest priority (fallback)
    
    static bool matches(const Slice& s, int len) {
        return true;  // Always matches as fallback
    }
};

/**
 * @brief Template specialization strategies for slice processing
 * 
 * Each strategy implements the specific slice processing logic
 * for its corresponding slice processing type.
 */
template<SliceProcessingType Type>
struct SliceProcessingStrategy {
    static std::expected<QJsonArray, EvalError> process(const QJsonArray& array, const Slice& s) {
        // Default implementation should never be called
        return std::unexpected(EvalError::InvalidSlice);
    }
};

// Zero step strategy: Return empty array (RFC 9535 compliance)
template<>
struct SliceProcessingStrategy<SliceProcessingType::ZeroStep> {
    static std::expected<QJsonArray, EvalError> process(const QJsonArray& array, const Slice& s) {
        qCDebug(jsonPathLog) << "evalSlice: zero step, returning empty array";
        return QJsonArray{};  // Empty result for zero step (RFC 9535 compliance)
    }
};

// Forward slice strategy: Positive step iteration
template<>
struct SliceProcessingStrategy<SliceProcessingType::ForwardSlice> {
    static std::expected<QJsonArray, EvalError> process(const QJsonArray& array, const Slice& s) {
        auto pooledArray = acquirePooledArray();
        QJsonArray& out = *pooledArray;

        const int len = array.size();
        constexpr qsizetype SENTINEL = std::numeric_limits<qsizetype>::max();

        qsizetype start = s.start;
        qsizetype stop = s.end;
        qsizetype step = s.step;

        // Default translation for forward iteration
        if (start == SENTINEL) start = 0;
        if (stop == SENTINEL) stop = len;

        // Convert negatives and clamp for forward iteration
        if (start < 0) start += len;
        if (stop < 0) stop += len;
        
        if (start < 0) start = 0;
        if (start > len) start = len;
        if (stop < 0) stop = 0;
        if (stop > len) stop = len;

        qCDebug(jsonPathLog) << "evalSlice forward: start=" << start << " stop=" << stop << " step=" << step;

        // Forward iteration
        for (qsizetype i = start; i < stop; i += step) {
            if (i >= 0 && i < len) {
                qCDebug(jsonPathLog) << "  visiting i=" << i;
                out.append(array[static_cast<int>(i)]);
            }
        }

        qCDebug(jsonPathLog) << "evalSlice result size=" << out.size();
        return QJsonArray(std::move(out));
    }
};

// Backward slice strategy: Negative step iteration
template<>
struct SliceProcessingStrategy<SliceProcessingType::BackwardSlice> {
    static std::expected<QJsonArray, EvalError> process(const QJsonArray& array, const Slice& s) {
        auto pooledArray = acquirePooledArray();
        QJsonArray& out = *pooledArray;

        const int len = array.size();
        constexpr qsizetype SENTINEL = std::numeric_limits<qsizetype>::max();

        qsizetype start = s.start;
        qsizetype stop = s.end;
        qsizetype step = s.step;

        // Default translation for backward iteration
        if (start == SENTINEL) start = len - 1;
        if (stop == SENTINEL) stop = -1;

        // Convert negatives and clamp for backward iteration
        if (start < 0) start += len;
        if (stop < 0) stop += len;
        
        if (start < -1) start = -1;
        if (start >= len) start = len - 1;
        if (stop < -1) stop = -1;
        if (stop >= len) stop = len - 1;

        // Special case: if original stop was SENTINEL, set to -1 after adjustments
        if (s.end == SENTINEL) stop = -1;

        qCDebug(jsonPathLog) << "evalSlice backward: start=" << start << " stop=" << stop << " step=" << step;

        // Backward iteration
        for (qsizetype i = start; i > stop; i += step) {
            if (i >= 0 && i < len) {
                qCDebug(jsonPathLog) << "  visiting i=" << i;
                out.append(array[static_cast<int>(i)]);
            }
            // Prevent underflow when i + step would wrap
            if (i < std::numeric_limits<qsizetype>::min() - step)
                break;
        }

        qCDebug(jsonPathLog) << "evalSlice result size=" << out.size();
        return QJsonArray(std::move(out));
    }
};

// Default slice strategy: Fallback implementation
template<>
struct SliceProcessingStrategy<SliceProcessingType::DefaultSlice> {
    static std::expected<QJsonArray, EvalError> process(const QJsonArray& array, const Slice& s) {
        qCDebug(jsonPathLog) << "evalSlice: using default strategy";
        // This should rarely be reached, but provides a safe fallback
        return QJsonArray{};
    }
};

/**
 * @brief Recursive template dispatch table for slice processing strategies
 * 
 * Uses variadic templates and fold expressions for priority-ordered dispatch.
 * Strategies are evaluated in priority order until a match is found.
 */
template<SliceProcessingType FirstType, SliceProcessingType... RestTypes>
struct SliceProcessingDispatchTable {
    static std::expected<QJsonArray, EvalError> dispatch(
        const QJsonArray& array, 
        const Slice& s) {
        
        // Try the first strategy
        if constexpr (SliceProcessingDef<FirstType>::enabled) {
            if (SliceProcessingDef<FirstType>::matches(s, array.size())) {
                return SliceProcessingStrategy<FirstType>::process(array, s);
            }
        }
        
        // Recurse to remaining types
        if constexpr (sizeof...(RestTypes) > 0) {
            return SliceProcessingDispatchTable<RestTypes...>::dispatch(array, s);
        } else {
            // No more strategies to try
            return std::unexpected(EvalError::InvalidSlice);
        }
    }
};

// Handle empty parameter pack
template<SliceProcessingType FirstType>
struct SliceProcessingDispatchTable<FirstType> {
    static std::expected<QJsonArray, EvalError> dispatch(
        const QJsonArray& array, 
        const Slice& s) {
        
        // Try the first strategy
        if constexpr (SliceProcessingDef<FirstType>::enabled) {
            if (SliceProcessingDef<FirstType>::matches(s, array.size())) {
                return SliceProcessingStrategy<FirstType>::process(array, s);
            }
        }
        
        // No more strategies to try
        return std::unexpected(EvalError::InvalidSlice);
    }
};

/**
 * @brief TableGen-inspired slice processing dispatcher
 * 
 * Defines the priority-ordered list of slice processing strategies
 * and provides the main dispatch entry point.
 */
struct SliceProcessingDispatcher {
    // Priority-ordered strategy list (highest priority first)
    using DispatchTable = SliceProcessingDispatchTable<
        SliceProcessingType::ZeroStep,        // Priority 1000
        SliceProcessingType::ForwardSlice,    // Priority 800
        SliceProcessingType::BackwardSlice,   // Priority 700
        SliceProcessingType::DefaultSlice     // Priority 100
    >;
    
    static std::expected<QJsonArray, EvalError> dispatch(const QJsonArray& array, const Slice& s) {
        return DispatchTable::dispatch(array, s);
    }
};

/**
 * @brief Refactored evalSlice using TableGen-inspired architecture
 * 
 * Replaces the monolithic conditional logic with elegant compile-time
 * dispatch based on slice processing strategies.
 */
std::expected<QJsonArray, EvalError> evalSlice(const QJsonArray& array, const Slice& s)
{
    // Use TableGen-inspired dispatch for zero-overhead strategy selection
    return SliceProcessingDispatcher::dispatch(array, s);
}

// ===========================================================================
//  TableGen-Inspired Union Processing Architecture
// ===========================================================================

/**
 * @brief Union processing strategy types for TableGen-inspired dispatch
 * 
 * Defines the different union processing strategies that can be applied
 * based on the characteristics of the union token list.
 */
enum class UnionProcessingType : std::uint8_t {
    EmptyUnion,      // Handle empty union token list (error case)
    SingleToken,     // Handle single token union (optimization case)  
    MultipleTokens,  // Handle multiple token union (standard case)
    AllTokensFailed  // Handle case where all tokens fail (error aggregation)
};

/**
 * @brief TableGen-style pattern definitions for union processing strategies
 * 
 * Each template specialization defines the characteristics and enabling
 * conditions for a specific union processing strategy.
 */
template<UnionProcessingType Type>
struct UnionProcessingDef {
    static constexpr bool enabled = false;
};

// Empty union strategy: Handle empty token lists
template<>
struct UnionProcessingDef<UnionProcessingType::EmptyUnion> {
    static constexpr bool enabled = true;
    
    static bool matches(const QVector<qsizetype>& unionTokens) {
        return unionTokens.isEmpty();
    }
};

// Single token strategy: Optimize single token unions
template<>
struct UnionProcessingDef<UnionProcessingType::SingleToken> {
    static constexpr bool enabled = true;
    
    static bool matches(const QVector<qsizetype>& unionTokens) {
        return unionTokens.size() == 1;
    }
};

// Multiple tokens strategy: Standard union processing
template<>
struct UnionProcessingDef<UnionProcessingType::MultipleTokens> {
    static constexpr bool enabled = true;
    
    static bool matches(const QVector<qsizetype>& unionTokens) {
        return unionTokens.size() > 1;
    }
};

/**
 * @brief Template specialization strategies for union processing
 * 
 * Each strategy implements the specific logic for processing unions
 * of the corresponding type with zero runtime overhead.
 */
template<UnionProcessingType Type>
struct UnionProcessingStrategy {
    static std::expected<QJsonArray, EvalError> process(
        const PathEvalCtx& ctx, 
        const QVector<qsizetype>& unionTokens, 
        const QJsonArray& working, 
        const QJsonValue& root)
    {
        // Default implementation should never be called
        return std::unexpected(EvalError::InvalidSlice);
    }
};

// Empty union strategy: Return appropriate error
template<>
struct UnionProcessingStrategy<UnionProcessingType::EmptyUnion> {
    static std::expected<QJsonArray, EvalError> process(
        const PathEvalCtx& ctx, 
        const QVector<qsizetype>& unionTokens, 
        const QJsonArray& working, 
        const QJsonValue& root)
    {
        
        qCDebug(jsonPathLog) << "[UnionProcessing::EmptyUnion] No tokens to process";
        return std::unexpected(EvalError::TypeMismatchObject);
    }
};

// Single token strategy: Optimized single token processing
template<>
struct UnionProcessingStrategy<UnionProcessingType::SingleToken> {
    static std::expected<QJsonArray, EvalError> process(
        const PathEvalCtx& ctx, 
        const QVector<qsizetype>& unionTokens, 
        const QJsonArray& working, 
        const QJsonValue& root)
    {
        
        qCDebug(jsonPathLog) << "[UnionProcessing::SingleToken] Processing single token optimization";
        
        qsizetype tokenIdx = unionTokens[0];
        auto tokenResult = processSingleUnionToken(ctx, tokenIdx, working, root);
        
        if (tokenResult.success) {
            qCDebug(jsonPathLog) << "[UnionProcessing::SingleToken] Single token succeeded with" << tokenResult.results.size() << "results";
            return tokenResult.results;
        } else {
            qCDebug(jsonPathLog) << "[UnionProcessing::SingleToken] Single token failed with error" << static_cast<int>(tokenResult.error);
            return std::unexpected(tokenResult.error);
        }
    }
};

// Multiple tokens strategy: Standard union processing with monadic error handling
template<>
struct UnionProcessingStrategy<UnionProcessingType::MultipleTokens> {
    static std::expected<QJsonArray, EvalError> process(
        const PathEvalCtx& ctx, 
        const QVector<qsizetype>& unionTokens, 
        const QJsonArray& working, 
        const QJsonValue& root)
    {
        
        qCDebug(jsonPathLog) << "[UnionProcessing::MultipleTokens] Processing" << unionTokens.size() << "tokens";
        
        // Monadic approach: Transform union tokens into results using monadic chaining
        QVector<QJsonArray> resultArrays;
        resultArrays.reserve(unionTokens.size());
        
        // Use monadic pattern to process each token and collect successful results
        auto processToken = [&](qsizetype tokenIdx) -> std::optional<QJsonArray> {
            qCDebug(jsonPathLog) << "[UnionProcessing::MultipleTokens] Processing token" << tokenIdx;
            
            auto tokenResult = processSingleUnionToken(ctx, tokenIdx, working, root);
            if (tokenResult.success) {
                qCDebug(jsonPathLog) << "[UnionProcessing::MultipleTokens] Token" << tokenIdx << "succeeded with" << tokenResult.results.size() << "results";
                return tokenResult.results;
            } else {
                qCDebug(jsonPathLog) << "[UnionProcessing::MultipleTokens] Token" << tokenIdx << "failed with error" << static_cast<int>(tokenResult.error) << "- continuing with other tokens";
                return std::nullopt; // Convert failure to empty optional for union semantics
            }
        };
        
        // Collect successful results using monadic transformation
        EvalError lastError = EvalError::TypeMismatchObject;
        for (qsizetype tokenIdx : unionTokens) {
            if (auto result = processToken(tokenIdx)) {
                resultArrays.append(*result);
            } else {
                // Track last error for potential failure case
                auto tokenResult = processSingleUnionToken(ctx, tokenIdx, working, root);
                if (!tokenResult.success) {
                    lastError = tokenResult.error;
                }
            }
        }
        
        // Monadic error handling: Only fail if ALL tokens failed
        if (resultArrays.isEmpty()) {
            qCDebug(jsonPathLog) << "[UnionProcessing::MultipleTokens] All tokens failed, returning last error" << static_cast<int>(lastError);
            return std::unexpected(lastError);
        }
        
        qCDebug(jsonPathLog) << "[UnionProcessing::MultipleTokens] Merging results from" << resultArrays.size() << "successful arrays";
        auto mergedResults = mergeTokenResults(resultArrays, root);
        qCDebug(jsonPathLog) << "[UnionProcessing::MultipleTokens] Merged to" << mergedResults.size() << "total results";
        return mergedResults;
    }
};

/**
 * @brief Recursive template dispatch table for union processing strategies
 * 
 * Implements priority-ordered dispatch using recursive template expansion.
 * Strategies are tried in order until one matches the union characteristics.
 */
template<UnionProcessingType FirstType, UnionProcessingType... RestTypes>
struct UnionProcessingDispatchTable {
    static std::expected<QJsonArray, EvalError> dispatch(
        const PathEvalCtx& ctx, 
        const QVector<qsizetype>& unionTokens, 
        const QJsonArray& working, 
        const QJsonValue& root) {
        
        // Try the first strategy
        if constexpr (UnionProcessingDef<FirstType>::enabled) {
            if (UnionProcessingDef<FirstType>::matches(unionTokens)) {
                return UnionProcessingStrategy<FirstType>::process(ctx, unionTokens, working, root);
            }
        }
        
        // Recurse to remaining types
        if constexpr (sizeof...(RestTypes) > 0) {
            return UnionProcessingDispatchTable<RestTypes...>::dispatch(ctx, unionTokens, working, root);
        } else {
            // No more strategies to try
            return std::unexpected(EvalError::InvalidSlice);
        }
    }
};

// Handle empty parameter pack
template<UnionProcessingType FirstType>
struct UnionProcessingDispatchTable<FirstType> {
    static std::expected<QJsonArray, EvalError> dispatch(
        const PathEvalCtx& ctx, 
        const QVector<qsizetype>& unionTokens, 
        const QJsonArray& working, 
        const QJsonValue& root) {
        
        // Try the first strategy
        if constexpr (UnionProcessingDef<FirstType>::enabled) {
            if (UnionProcessingDef<FirstType>::matches(unionTokens)) {
                return UnionProcessingStrategy<FirstType>::process(ctx, unionTokens, working, root);
            }
        }
        
        // No more strategies to try
        return std::unexpected(EvalError::InvalidSlice);
    }
};

/**
 * @brief TableGen-inspired union processing dispatcher
 * 
 * Defines the priority-ordered list of union processing strategies
 * for compile-time dispatch resolution.
 */
using UnionProcessingDispatcher = UnionProcessingDispatchTable<
    UnionProcessingType::EmptyUnion,      // Highest priority: handle empty unions first
    UnionProcessingType::SingleToken,     // High priority: optimize single token case
    UnionProcessingType::MultipleTokens   // Standard priority: general multiple token case
>;

/**
 * @brief Refactored processUnionTokens using TableGen-inspired architecture
 * 
 * Replaces the monolithic conditional logic with elegant compile-time
 * dispatch based on union processing strategies.
 */
std::expected<QJsonArray, EvalError> processUnionTokens(
    const PathEvalCtx& ctx, 
    const QVector<qsizetype>& unionTokens, 
    const QJsonArray& working, 
    const QJsonValue& root)
{
    qCDebug(jsonPathLog) << "[processUnionTokens] Starting TableGen-inspired dispatch with" << unionTokens.size() << "tokens";
    
    // Use TableGen-inspired dispatch for zero-overhead strategy selection
    return UnionProcessingDispatcher::dispatch(ctx, unionTokens, working, root);
}

// ---------------------------------------------------------------------------
//  Union detection and processing helpers
// ---------------------------------------------------------------------------

// Helper: Check if a token is a selector token that can appear in unions
bool isSelectorToken(const Token& token) {
    return token.kind == Token::Kind::Index || token.kind == Token::Kind::Key ||
           token.kind == Token::Kind::Filter || token.kind == Token::Kind::Wildcard ||
           token.kind == Token::Kind::Slice;
}

// Helper: Collect consecutive selector tokens starting from a given index
QVector<qsizetype> collectConsecutiveSelectorTokens(const PathEvalCtx& ctx, qsizetype startIndex) {
    QVector<qsizetype> unionTokens;
    unionTokens.append(startIndex);
    
    qsizetype j = startIndex + 1;
    while (j < ctx.tokens.size()) {
        const Token& nextTk = ctx.tokens[j];
        if (isSelectorToken(nextTk)) {
            unionTokens.append(j);
            ++j;
        } else {
            break; // Stop at non-selector tokens (e.g., Recursive)
        }
    }
    
    return unionTokens;
}

// Helper: Check if all tokens are from the same bracket group
bool areTokensFromSameBracketGroup(const PathEvalCtx& ctx, const QVector<qsizetype>& tokenIndices) {
    if (tokenIndices.size() <= 1) return false;
    
    int firstBracketGroupId = ctx.tokens[tokenIndices[0]].bracketGroupId;
    
    // If first token is not from a bracket, this can't be a bracket union
    if (firstBracketGroupId <= 0) {
        return false;
    }
    
    // Check if all subsequent tokens have the same bracket group ID
    for (qsizetype i = 1; i < tokenIndices.size(); ++i) {
        if (ctx.tokens[tokenIndices[i]].bracketGroupId != firstBracketGroupId) {
            return false;
        }
    }
    
    return true;
}

UnionDetectionResult detectUnionTokens(const PathEvalCtx& ctx, qsizetype startIndex) {
    QVector<qsizetype> unionTokens = collectConsecutiveSelectorTokens(ctx, startIndex);
    bool shouldUseUnion = areTokensFromSameBracketGroup(ctx, unionTokens);
    qsizetype nextIndex = startIndex + unionTokens.size();
    
    return {unionTokens, shouldUseUnion, nextIndex};
}

// Helper: Process a single union token and collect its results
TokenProcessingResult processSingleUnionToken(
    const PathEvalCtx& ctx, 
    qsizetype tokenIdx, 
    const QJsonArray& working, 
    const QJsonValue& root)
{
    qCDebug(jsonPathLog) << "[processSingleUnionToken] Processing token" << tokenIdx << "with" << working.size() << "working values";
    
    // Use pooled array for efficient result collection
    auto pooledArray = acquirePooledArray();
    QJsonArray& results = *pooledArray;
    
    // Process each working value using monadic pattern
    auto processWorkingValue = [&](const QJsonValue& workingValue) -> std::expected<QJsonArray, EvalError> {
        return evaluateToken(ctx, ctx.tokens[tokenIdx], workingValue);
    };
    
    // Aggregate results using monadic error handling
    auto aggregateResults = [&]() -> std::expected<QJsonArray, EvalError> {
        for (qsizetype i = 0; i < working.size(); ++i) {
            const auto& workingValue = working[i];
            qCDebug(jsonPathLog) << "[processSingleUnionToken] Processing working value" << i << "of type" << static_cast<int>(workingValue.type());
            
            // Use monadic chaining for token evaluation
            auto tokenResult = processWorkingValue(workingValue);
            if (!tokenResult) {
                qCDebug(jsonPathLog) << "[processSingleUnionToken] Token evaluation failed with error" << static_cast<int>(tokenResult.error());
                return std::unexpected(tokenResult.error());
            }
            
            qCDebug(jsonPathLog) << "[processSingleUnionToken] Token evaluation succeeded with" << tokenResult->size() << "results";
            // Append results from this token using range-based iteration
            for (const auto& value : *tokenResult) {
                results.append(value);
            }
        }
        return results;
    };
    
    // Convert std::expected result to TokenProcessingResult using monadic pattern
    return aggregateResults()
        .transform([&](const QJsonArray& successResults) -> TokenProcessingResult {
            qCDebug(jsonPathLog) << "[processSingleUnionToken] Completed with" << successResults.size() << "total results";
            return TokenProcessingResult{
                .success = true,
                .results = QJsonArray(successResults), // Copy from pooled array
                .error = EvalError::KeyNotFound // Unused for success case
            };
        })
        .or_else([](EvalError error) -> std::expected<TokenProcessingResult, EvalError> {
            return TokenProcessingResult{
                .success = false,
                .results = QJsonArray{},
                .error = error
            };
        })
        .value(); // Safe to call value() since or_else always returns a value
}

// Helper: Merge results from multiple tokens using simple concatenation
QJsonArray mergeTokenResults(const QVector<QJsonArray>& resultArrays, const QJsonValue& root) {
    QJsonArray collectedResults;
    
    // Simple concatenation approach - avoid complex cursor logic that may cause infinite loops
    for (const auto& results : resultArrays) {
        for (const auto& value : results) {
            collectedResults.append(value);
        }
    }
    
    return collectedResults;
}

// Helper: Collect keys from consecutive Key/KeyList tokens
KeyCollectionResult collectKeysFromTokens(const PathEvalCtx& ctx, qsizetype startIndex)
{
    QStringList keys;
    qsizetype i = startIndex;
    
    while (i < ctx.tokens.size()) {
        const Token& token = ctx.tokens[i];
        if (token.kind == Token::Kind::Key) {
            keys.append(token.key);
            ++i;
        } else if (token.kind == Token::Kind::KeyList) {
            keys.append(token.key.split(u'\n'));
            ++i;
        } else {
            break;
        }
    }
    
    return {keys, i};
}

// Helper: Check if an object contains all required keys
bool objectContainsAllKeys(const QJsonObject& obj, const QStringList& keys)
{
    for (const QString& key : keys) {
        if (!obj.contains(key)) {
            return false;
        }
    }
    return true;
}

// Helper: Process object for leaf selection
void processObjectForLeafSelection(const QJsonObject& obj, const QStringList& keys, const QJsonValue& v, QJsonArray* results)
{
    if (!objectContainsAllKeys(obj, keys)) {
        return;
    }
    
    if (keys.size() > 1) {
        // Multi-property leaf union ⇒ return parent only (Jayway)
        results->append(v);
    } else {
        // Single-property leaf: return only the value, not the parent
        results->append(obj.value(keys.first()));
    }
}

// Helper: Process object for non-leaf selection
void processObjectForNonLeafSelection(const QJsonObject& obj, const QStringList& keys, const QJsonValue& v, QJsonArray* results)
{
    for (const QString& key : keys) {
        if (obj.contains(key)) {
            results->append(obj[key]);
        }
    }
}

// Helper: Deduplicate JSON values using hash-based approach
QJsonArray deduplicateJsonValues(const QJsonArray& input, const QJsonValue& root) {
    QSet<uint> seen;
    QJsonArray dedup;
    
    auto workingDedupCursor = json_query::json_path::internal::makeSimpleContextCursor(input, root, root);
    for (const auto& [v2, context] : workingDedupCursor) {
        if (v2.isObject()) {
            uint h = qHash(QJsonDocument(v2.toObject()).toJson());
            if (seen.contains(h)) continue;
            seen.insert(h);
        } else if (v2.isArray()) {
            uint h = qHash(QJsonDocument(v2.toArray()).toJson());
            if (seen.contains(h)) continue;
            seen.insert(h);
        }
        dedup.append(v2);
    }
    
    return dedup;
}

std::expected<QJsonArray, EvalError> processBranchUniqueSelection(
    const PathEvalCtx& ctx,
    qsizetype& i,
    const QJsonArray& working,
    const QJsonValue& root,
    bool isLeaf)
{
    auto keyResult = collectKeysFromTokens(ctx, i);
    const QStringList& keys = keyResult.keys;
    i = keyResult.nextIndex;
    
    if (keys.isEmpty()) {
        return std::unexpected(EvalError::KeyNotFound);
    }
    
    auto pooledArray = acquirePooledArray();
    QJsonArray& results = *pooledArray;
    
    for (const auto& workingValue : working) {
        if (!workingValue.isObject()) continue;
        
        const QJsonObject obj = workingValue.toObject();
        
        if (isLeaf) {
            processObjectForLeafSelection(obj, keys, workingValue, &results);
        } else {
            processObjectForNonLeafSelection(obj, keys, workingValue, &results);
        }
    }
    
    // Deduplicate results
    return deduplicateJsonValues(results, root);
}

bool addsMultiplicity(const Token& tk)
{
    switch (tk.kind) {
    case Token::Kind::Wildcard:
    case Token::Kind::Recursive:
    case Token::Kind::KeyList:
        return true;
    default:
        return false;
    }
}

QJsonValue squash(QJsonArray arr, bool multi)
{
    if (multi || arr.size() != 1)
        return arr;
    return arr[0];
}

QJsonValue applyTrailing(json_path::FunctionType fn, const QJsonValue& v)
{
    using enum json_path::FunctionType;

    switch (fn) {
    case None:   return v;

    case Length:
        if (v.isArray())  return v.toArray().size();
        if (v.isObject()) return v.toObject().size();
        return 0;

    case Min:
    case Max:
        if (!v.isArray()) return QJsonValue(QJsonValue::Undefined);
        {
            const auto arr = v.toArray();
            bool first=true; double best=0.0;
            for (const auto& e : arr) {
                if (!e.isDouble()) continue;
                const double d = e.toDouble();
                if (first || (fn==Min ? d<best : d>best))
                    best = d, first = false;
            }
            return first ? QJsonValue(QJsonValue::Undefined)
                         : QJsonValue(best);
        }
    }
    std::unreachable();
}

} // namespace json_query::json_path::detail
