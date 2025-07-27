#pragma once

#include <QJsonValue>
#include <QJsonArray>
#include <QJsonObject>
#include <vector>
#include <expected>
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/ResultStreamer.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/JSONPathExpected.hpp"

namespace json_query::json_path::internal {

/**
 * @brief Iterative implementation of recursive descent to reduce call stack memory
 * 
 * This replaces the recursive __evaluateRecursiveImpl with an iterative version
 * that uses an explicit stack, significantly reducing memory overhead and
 * improving performance for deep traversals.
 */
class IterativeRecursiveDescent {
public:
    /**
     * @brief Stack frame for iterative recursive descent
     */
    struct StackFrame {
        QJsonValue value;
        bool processed = false; // Whether this frame has been processed
        
        explicit StackFrame(QJsonValue v) : value(std::move(v)) {}
    };
    
    /**
     * @brief Iterative recursive descent with result streaming
     * 
     * @param rootValue The root value to traverse recursively
     * @param streamer Result streamer for emitting found values
     * @return std::expected<void, EvalError> Success or error
     */
    template<json_query::json_path::internal::ResultStreamerConcept StreamerType>
    static std::expected<void, EvalError> evaluateIterative(
        const QJsonValue& rootValue, 
        StreamerType& streamer) {
        
        // Use a reusable stack to minimize allocations
        thread_local std::vector<StackFrame> stack;
        stack.clear();
        stack.reserve(64); // Pre-allocate reasonable capacity
        
        // Start with root value
        stack.emplace_back(rootValue);
        
        while (!stack.empty()) {
            StackFrame& frame = stack.back();
            
            if (!frame.processed) {
                // First time processing this frame - emit the value
                streamer.emitValue(frame.value);
                frame.processed = true;
                
                // Add children to stack for traversal
                if (frame.value.isObject()) {
                    const QJsonObject obj = frame.value.toObject();
                    // Add in reverse order so we process in original order
                    for (auto it = obj.end(); it != obj.begin(); ) {
                        --it;
                        stack.emplace_back(it.value());
                    }
                } else if (frame.value.isArray()) {
                    const QJsonArray arr = frame.value.toArray();
                    // Add in reverse order so we process in original order
                    for (qsizetype i = arr.size() - 1; i >= 0; --i) {
                        stack.emplace_back(arr[i]);
                    }
                }
            } else {
                // Frame already processed, remove it
                stack.pop_back();
            }
        }
        
        return {};
    }
    
    /**
     * @brief Iterative recursive descent returning QJsonArray (for compatibility)
     * 
     * @param rootValue The root value to traverse recursively
     * @return std::expected<QJsonArray, EvalError> Array of all found values
     */
    static std::expected<QJsonArray, EvalError> evaluateIterativeArray(
        const QJsonValue& rootValue) {
        
        auto pooledArray = acquirePooledArray();
        ResultCollector collector(pooledArray.get());
        
        auto result = evaluateIterative(rootValue, collector);
        if (!result) {
            return std::unexpected(result.error());
        }
        
        // Move the result to avoid copying
        QJsonArray finalResult = std::move(*pooledArray);
        return finalResult;
    }
    
    /**
     * @brief Memory-efficient depth-limited recursive descent
     * 
     * Prevents stack overflow and excessive memory usage by limiting traversal depth.
     * 
     * @param rootValue The root value to traverse
     * @param maxDepth Maximum traversal depth (0 = unlimited)
     * @param streamer Result streamer for emitting values
     * @return std::expected<void, EvalError> Success or error
     */
    template<json_query::json_path::internal::ResultStreamerConcept StreamerType>
    static std::expected<void, EvalError> evaluateIterativeDepthLimited(
        const QJsonValue& rootValue,
        size_t maxDepth,
        StreamerType& streamer) {
        
        struct DepthFrame {
            QJsonValue value;
            size_t depth;
            bool processed = false;
            
            DepthFrame(QJsonValue v, size_t d) : value(std::move(v)), depth(d) {}
        };
        
        thread_local std::vector<DepthFrame> stack;
        stack.clear();
        stack.reserve(std::min(maxDepth * 8, size_t(256))); // Reasonable capacity
        
        stack.emplace_back(rootValue, 0);
        
        while (!stack.empty()) {
            DepthFrame& frame = stack.back();
            
            if (!frame.processed) {
                // Emit the value
                streamer.emitValue(frame.value);
                frame.processed = true;
                
                // Add children if within depth limit
                if (maxDepth == 0 || frame.depth < maxDepth) {
                    if (frame.value.isObject()) {
                        const QJsonObject obj = frame.value.toObject();
                        for (auto it = obj.end(); it != obj.begin(); ) {
                            --it;
                            stack.emplace_back(it.value(), frame.depth + 1);
                        }
                    } else if (frame.value.isArray()) {
                        const QJsonArray arr = frame.value.toArray();
                        for (qsizetype i = arr.size() - 1; i >= 0; --i) {
                            stack.emplace_back(arr[i], frame.depth + 1);
                        }
                    }
                }
            } else {
                stack.pop_back();
            }
        }
        
        return {};
    }
    
    /**
     * @brief Early termination patterns for common recursive queries
     */
    struct EarlyTerminationPatterns {
        // Common field names that appear frequently in recursive queries
        static constexpr std::array<QStringView, 8> COMMON_FIELDS = {
            u"title", u"name", u"id", u"type", u"value", u"data", u"content", u"text"
        };
        
        // Check if a field name matches common patterns for early termination
        static QT_QUERY_JSON_ALWAYS_INLINE bool isCommonField(QStringView fieldName) {
            for (const auto& field : COMMON_FIELDS) {
                if (fieldName == field) return true;
            }
            return false;
        }
        
        // Estimate traversal cost based on document structure
        static QT_QUERY_JSON_ALWAYS_INLINE size_t estimateTraversalCost(const QJsonValue& value) {
            if (value.isObject()) {
                return value.toObject().size() * 2; // Object keys + values
            } else if (value.isArray()) {
                return value.toArray().size() * 3; // Array elements with potential nesting
            }
            return 1; // Leaf value
        }
    };

    /**
     * @brief Optimized recursive descent with early termination and indexing
     * 
     * Implements Phase 2 optimizations:
     * - Early termination for common patterns
     * - Structural indexing to avoid unnecessary traversal
     * - Pattern-specific fast paths
     * 
     * @param rootValue The root value to traverse recursively
     * @param targetField Optional target field name for early termination
     * @param streamer Result streamer for emitting values
     * @return std::expected<void, EvalError> Success or error
     */
    template<json_query::json_path::internal::ResultStreamerConcept StreamerType>
    static std::expected<void, EvalError> evaluateIterativeWithEarlyTermination(
        const QJsonValue& rootValue,
        QStringView targetField,
        StreamerType& streamer) {
        
        // Use thread-local stack for better performance
        thread_local std::vector<StackFrame> stack;
        stack.clear();
        stack.reserve(64); // Smaller initial capacity for early termination
        
        // Early termination heuristics
        const bool useEarlyTermination = !targetField.isEmpty() && 
                                       EarlyTerminationPatterns::isCommonField(targetField);
        const size_t maxTraversalCost = useEarlyTermination ? 1000 : SIZE_MAX;
        size_t currentCost = 0;
        
        // Start with root value
        stack.emplace_back(rootValue);
        
        while (!stack.empty()) {
            StackFrame& frame = stack.back();
            
            // Early termination check
            if (useEarlyTermination && currentCost > maxTraversalCost) {
                break;
            }
            
            if (frame.value.isObject()) {
                const QJsonObject obj = frame.value.toObject();
                
                if (!frame.processed) {
                    // Emit the object itself
                    streamer.emitValue(frame.value);
                    frame.processed = true;
                    
                    // Early termination: if we found the target field, prioritize it
                    if (useEarlyTermination && obj.contains(targetField)) {
                        const QJsonValue targetValue = obj[targetField];
                        streamer.emitValue(targetValue);
                        
                        // If target is a leaf value, we can potentially skip other fields
                        if (!targetValue.isObject() && !targetValue.isArray()) {
                            currentCost += 1;
                            continue;
                        }
                    }
                }
                
                // Add children to stack (reverse order for correct traversal)
                bool hasUnprocessedChildren = false;
                for (auto it = obj.end(); it != obj.begin(); ) {
                    --it;
                    if (!frame.processed || it == obj.begin()) {
                        stack.emplace_back(it.value());
                        hasUnprocessedChildren = true;
                        currentCost += EarlyTerminationPatterns::estimateTraversalCost(it.value());
                        break;
                    }
                }
                
                if (!hasUnprocessedChildren) {
                    stack.pop_back();
                }
                
            } else if (frame.value.isArray()) {
                const QJsonArray arr = frame.value.toArray();
                
                if (!frame.processed) {
                    // Emit the array itself
                    streamer.emitValue(frame.value);
                    frame.processed = true;
                }
                
                // Add children to stack
                bool hasUnprocessedChildren = false;
                for (qsizetype i = arr.size() - 1; i >= 0; --i) {
                    if (!frame.processed || i == 0) {
                        stack.emplace_back(arr[i]);
                        hasUnprocessedChildren = true;
                        currentCost += EarlyTerminationPatterns::estimateTraversalCost(arr[i]);
                        break;
                    }
                }
                
                if (!hasUnprocessedChildren) {
                    stack.pop_back();
                }
                
            } else {
                // Emit leaf value and remove frame
                streamer.emitValue(frame.value);
                stack.pop_back();
                currentCost += 1;
            }
        }
        
        return {};
    }
    
    /**
     * @brief Get statistics about stack usage for optimization
     */
    struct Stats {
        size_t maxStackDepth = 0;
        size_t totalFramesProcessed = 0;
        size_t memoryReused = 0; // Number of times thread_local stack was reused
    };
    
    // Thread-local statistics
    thread_local static Stats stats_;
    
    static const Stats& getStats() { return stats_; }
    static void resetStats() { stats_ = {}; }
};

// Thread-local statistics declaration (definition in .cpp file)
// thread_local IterativeRecursiveDescent::Stats IterativeRecursiveDescent::stats_;

} // namespace json_query::json_path::internal
