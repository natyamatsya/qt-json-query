#pragma once

#include "json-query/Common.h"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include <expected>
#include <QtCore/QJsonValue>
#include <QtCore/QJsonObject>
#include <QtCore/QJsonArray>

namespace json_query::json_path::internal {

/**
 * @brief Path pattern specializations for common JSONPath usage patterns
 * 
 * This system provides highly optimized fast paths for the most common JSONPath
 * patterns, eliminating the overhead of generic token-by-token evaluation for
 * patterns that can be resolved more efficiently.
 */

/**
 * @brief Path pattern classification for compile-time optimization
 */
enum class PathPattern {
    SimpleKey,          // $.key
    NestedKeys,         // $.key1.key2.key3
    ArrayIndex,         // $.array[0]
    ArrayWildcard,      // $.array[*]
    KeyThenIndex,       // $.key[0]
    IndexThenKey,       // $[0].key
    WildcardThenKey,    // $.*.key
    Generic             // Complex patterns requiring generic evaluation
};

/**
 * @brief Path pattern detector for compile-time classification
 * 
 * Analyzes token sequences to identify common patterns that can be optimized
 * with specialized fast-path implementations.
 */
class PathPatternDetector {
public:
    /**
     * @brief Detect the pattern type of a token sequence
     * 
     * @param tokens The token sequence to analyze
     * @return The detected pattern type
     */
    QT_QUERY_JSON_ALWAYS_INLINE static PathPattern detectPattern(const QVector<Token>& tokens) noexcept {
        if (tokens.isEmpty()) {
            return PathPattern::Generic;
        }
        
        // Conservative pattern detection - only optimize simple, safe cases
        // Avoid patterns that might involve complex string processing, escaping, or edge cases
        
        // Simple key access: $.key (only for simple alphanumeric keys)
        if (tokens.size() == 1 && tokens[0].kind == Token::Kind::Key) {
            const QString& key = tokens[0].key;
            // Only optimize simple keys without special characters, quotes, or escape sequences
            if (isSimpleKey(key)) {
                return PathPattern::SimpleKey;
            }
        }
        
        // Nested keys: $.key1.key2.key3... (only for simple keys)
        if (tokens.size() >= 2 && tokens.size() <= 3) { // Reduced from 4 to 3 for safety
            bool allSimpleKeys = true;
            for (const auto& token : tokens) {
                if (token.kind != Token::Kind::Key || !isSimpleKey(token.key)) {
                    allSimpleKeys = false;
                    break;
                }
            }
            if (allSimpleKeys) {
                return PathPattern::NestedKeys;
            }
        }
        
        // Array index: $[0] (only for simple positive indices)
        if (tokens.size() == 1 && tokens[0].kind == Token::Kind::Index) {
            // Only optimize simple positive indices
            if (tokens[0].index >= 0) {
                return PathPattern::ArrayIndex;
            }
        }
        
        // For now, disable other patterns to ensure correctness
        // Array wildcard, combined patterns can be re-enabled after thorough testing
        
        // Default to generic evaluation for safety
        return PathPattern::Generic;
    }

private:
    /**
     * @brief Check if a key is simple enough for pattern specialization
     * 
     * Simple keys are alphanumeric with underscores, no quotes, no escape sequences,
     * no special characters that might require complex parsing.
     * 
     * Optimized with caching and forced inlining to address Qt string operation
     * inlining failures (QString::contains cost 65 vs threshold 45).
     */
    QT_QUERY_JSON_ALWAYS_INLINE static bool isSimpleKey(const QString& key) noexcept {
        if (key.isEmpty()) {
            return false;
        }
        
        // Fast path: check length first to avoid expensive operations
        if (key.length() > 64) { // Reasonable limit for simple keys
            return false;
        }
        
        // Optimized character validation with early exit
        for (const QChar& ch : key) {
            const ushort unicode = ch.unicode();
            // Fast check using unicode values to avoid Qt method calls
            if (!((unicode >= 'a' && unicode <= 'z') ||
                  (unicode >= 'A' && unicode <= 'Z') ||
                  (unicode >= '0' && unicode <= '9') ||
                  unicode == '_' || unicode == '-')) {
                return false; // Contains special characters
            }
        }
        
        // Additional fast checks for problematic characters using direct comparison
        // This avoids the costly QString::contains calls that were failing to inline
        const QChar* data = key.constData();
        const int len = key.length();
        for (int i = 0; i < len; ++i) {
            const ushort unicode = data[i].unicode();
            if (unicode == '\\' || unicode == '"' || unicode == '\'' || 
                unicode == ' ' || unicode == '\t' || unicode == '\n' ||
                unicode == '$' || unicode == '[' || unicode == ']') {
                return false; // Contains potentially problematic characters
            }
        }
        
        return true;
    }
};

/**
 * @brief Specialized evaluators for common path patterns
 * 
 * Each specialization provides a highly optimized implementation for a specific
 * pattern, eliminating the overhead of generic token-by-token evaluation.
 */
template<PathPattern Pattern>
struct PathPatternEvaluator {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& ctx,
        const QVector<Token>& tokens,
        const QJsonValue& root) noexcept 
    {
        // Default implementation falls back to generic evaluation
        return std::unexpected(EvalError::TypeMismatchObject);
    }
};

/**
 * @brief Simple key access optimization: $.key
 */
template<>
struct PathPatternEvaluator<PathPattern::SimpleKey> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/,
        const QVector<Token>& tokens,
        const QJsonValue& root) noexcept 
    {
        // Fast path: direct object property lookup
        if (!root.isObject()) {
            return QJsonArray{}; // Empty result for non-objects
        }
        
        const QJsonObject obj = root.toObject();
        const QString& key = tokens[0].key;
        const auto it = obj.find(key);
        
        if (it == obj.end()) {
            return QJsonArray{}; // Key not found
        }
        
        // Use ArrayPool for result optimization
        auto pooledArray = acquirePooledArray();
        QJsonArray& result = *pooledArray;
        result.append(it.value());
        
        return QJsonArray(result);
    }
};

/**
 * @brief Nested keys optimization: $.key1.key2.key3
 */
template<>
struct PathPatternEvaluator<PathPattern::NestedKeys> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/,
        const QVector<Token>& tokens,
        const QJsonValue& root) noexcept 
    {
        // Fast path: chained object navigation without intermediate arrays
        QJsonValue current = root;
        
        for (const auto& token : tokens) {
            if (!current.isObject()) {
                return QJsonArray{}; // Navigation failed
            }
            
            const QJsonObject obj = current.toObject();
            const auto it = obj.find(token.key);
            
            if (it == obj.end()) {
                return QJsonArray{}; // Key not found
            }
            
            current = it.value();
        }
        
        // Use ArrayPool for result optimization
        auto pooledArray = acquirePooledArray();
        QJsonArray& result = *pooledArray;
        result.append(current);
        
        return QJsonArray(result);
    }
};

/**
 * @brief Array index optimization: $[0] or $.array[0]
 */
template<>
struct PathPatternEvaluator<PathPattern::ArrayIndex> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/,
        const QVector<Token>& tokens,
        const QJsonValue& root) noexcept 
    {
        // Fast path: direct array element access
        if (!root.isArray()) {
            return QJsonArray{}; // Empty result for non-arrays
        }
        
        const QJsonArray arr = root.toArray();
        const int len = arr.size();
        
        // Normalize negative indices
        int index = tokens[0].index;
        if (index < 0) {
            index += len;
        }
        
        // Bounds check
        if (index < 0 || index >= len) {
            return QJsonArray{}; // Index out of bounds
        }
        
        // Use ArrayPool for result optimization
        auto pooledArray = acquirePooledArray();
        QJsonArray& result = *pooledArray;
        result.append(arr[index]);
        
        return QJsonArray(result);
    }
};

/**
 * @brief Array wildcard optimization: $[*] or $.array[*]
 */
template<>
struct PathPatternEvaluator<PathPattern::ArrayWildcard> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/,
        const QVector<Token>& /*tokens*/,
        const QJsonValue& root) noexcept 
    {
        // Fast path: return all array elements or object values
        if (root.isArray()) {
            // For arrays, return all elements
            const QJsonArray arr = root.toArray();
            
            // Use ArrayPool for result optimization
            auto pooledArray = acquirePooledArray();
            QJsonArray& result = *pooledArray;
            
            for (const auto& item : arr) {
                result.append(item);
            }
            
            return QJsonArray(result);
        } else if (root.isObject()) {
            // For objects, return all values
            const QJsonObject obj = root.toObject();
            
            // Use ArrayPool for result optimization
            auto pooledArray = acquirePooledArray();
            QJsonArray& result = *pooledArray;
            
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                result.append(it.value());
            }
            
            return QJsonArray(result);
        }
        
        return QJsonArray{}; // Empty result for other types
    }
};

/**
 * @brief Key then index optimization: $.key[0]
 */
template<>
struct PathPatternEvaluator<PathPattern::KeyThenIndex> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/,
        const QVector<Token>& tokens,
        const QJsonValue& root) noexcept 
    {
        // Fast path: object property access followed by array indexing
        if (!root.isObject()) {
            return QJsonArray{}; // Empty result for non-objects
        }
        
        const QJsonObject obj = root.toObject();
        const QString& key = tokens[0].key;
        const auto it = obj.find(key);
        
        if (it == obj.end()) {
            return QJsonArray{}; // Key not found
        }
        
        const QJsonValue& arrayValue = it.value();
        if (!arrayValue.isArray()) {
            return QJsonArray{}; // Property is not an array
        }
        
        const QJsonArray arr = arrayValue.toArray();
        const int len = arr.size();
        
        // Normalize negative indices
        int index = tokens[1].index;
        if (index < 0) {
            index += len;
        }
        
        // Bounds check
        if (index < 0 || index >= len) {
            return QJsonArray{}; // Index out of bounds
        }
        
        // Use ArrayPool for result optimization
        auto pooledArray = acquirePooledArray();
        QJsonArray& result = *pooledArray;
        result.append(arr[index]);
        
        return QJsonArray(result);
    }
};

/**
 * @brief Index then key optimization: $[0].key
 */
template<>
struct PathPatternEvaluator<PathPattern::IndexThenKey> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/,
        const QVector<Token>& tokens,
        const QJsonValue& root) noexcept 
    {
        // Fast path: array indexing followed by object property access
        if (!root.isArray()) {
            return QJsonArray{}; // Empty result for non-arrays
        }
        
        const QJsonArray arr = root.toArray();
        const int len = arr.size();
        
        // Normalize negative indices
        int index = tokens[0].index;
        if (index < 0) {
            index += len;
        }
        
        // Bounds check
        if (index < 0 || index >= len) {
            return QJsonArray{}; // Index out of bounds
        }
        
        const QJsonValue& objValue = arr[index];
        if (!objValue.isObject()) {
            return QJsonArray{}; // Array element is not an object
        }
        
        const QJsonObject obj = objValue.toObject();
        const QString& key = tokens[1].key;
        const auto it = obj.find(key);
        
        if (it == obj.end()) {
            return QJsonArray{}; // Key not found
        }
        
        // Use ArrayPool for result optimization
        auto pooledArray = acquirePooledArray();
        QJsonArray& result = *pooledArray;
        result.append(it.value());
        
        return QJsonArray(result);
    }
};

/**
 * @brief Wildcard then key optimization: $.*.key
 */
template<>
struct PathPatternEvaluator<PathPattern::WildcardThenKey> {
    static std::expected<QJsonArray, EvalError> eval(
        const detail::PathEvalCtx& /*ctx*/,
        const QVector<Token>& tokens,
        const QJsonValue& root) noexcept 
    {
        // Fast path: wildcard expansion followed by key extraction
        auto pooledArray = acquirePooledArray();
        QJsonArray& result = *pooledArray;
        
        const QString& key = tokens[1].key;
        
        if (root.isArray()) {
            // For arrays, check each element for the key
            const QJsonArray arr = root.toArray();
            for (const auto& item : arr) {
                if (item.isObject()) {
                    const QJsonObject obj = item.toObject();
                    const auto it = obj.find(key);
                    if (it != obj.end()) {
                        result.append(it.value());
                    }
                }
            }
        } else if (root.isObject()) {
            // For objects, check each value for the key
            const QJsonObject rootObj = root.toObject();
            for (auto it = rootObj.begin(); it != rootObj.end(); ++it) {
                const QJsonValue& value = it.value();
                if (value.isObject()) {
                    const QJsonObject obj = value.toObject();
                    const auto keyIt = obj.find(key);
                    if (keyIt != obj.end()) {
                        result.append(keyIt.value());
                    }
                }
            }
        }
        
        return QJsonArray(result);
    }
};

/**
 * @brief Pattern-aware path evaluator dispatcher
 * 
 * Detects common path patterns and dispatches to optimized implementations,
 * falling back to generic evaluation for complex patterns.
 */
class PatternAwarePathEvaluator {
public:
    /**
     * @brief Evaluate a path using pattern-specific optimizations
     * 
     * @param ctx Path evaluation context
     * @param tokens Token sequence representing the path
     * @param root Root JSON value to evaluate against
     * @return Expected result or error
     */
    static std::expected<QJsonArray, EvalError> evaluate(
        const detail::PathEvalCtx& ctx,
        const QVector<Token>& tokens,
        const QJsonValue& root) noexcept 
    {
        // Detect the pattern type
        const PathPattern pattern = PathPatternDetector::detectPattern(tokens);
        
        // Dispatch to specialized evaluator
        switch (pattern) {
            case PathPattern::SimpleKey:
                return PathPatternEvaluator<PathPattern::SimpleKey>::eval(ctx, tokens, root);
            case PathPattern::NestedKeys:
                return PathPatternEvaluator<PathPattern::NestedKeys>::eval(ctx, tokens, root);
            case PathPattern::ArrayIndex:
                return PathPatternEvaluator<PathPattern::ArrayIndex>::eval(ctx, tokens, root);
            case PathPattern::ArrayWildcard:
                return PathPatternEvaluator<PathPattern::ArrayWildcard>::eval(ctx, tokens, root);
            case PathPattern::KeyThenIndex:
                return PathPatternEvaluator<PathPattern::KeyThenIndex>::eval(ctx, tokens, root);
            case PathPattern::IndexThenKey:
                return PathPatternEvaluator<PathPattern::IndexThenKey>::eval(ctx, tokens, root);
            case PathPattern::WildcardThenKey:
                return PathPatternEvaluator<PathPattern::WildcardThenKey>::eval(ctx, tokens, root);
            case PathPattern::Generic:
            default:
                // Fall back to generic token-by-token evaluation
                return std::unexpected(EvalError::TypeMismatchObject);
        }
    }
};

} // namespace json_query::json_path::internal
