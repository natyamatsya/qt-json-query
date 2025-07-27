#include "json-query/json-path/JSONPathWildcardRecursive.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include "json-query/json-path/internal/ArrayPool.hpp"
#include "json-query/json-path/internal/IterativeRecursiveDescent.hpp"
#include <QDebug>
#include <QStringView>
#include <stdcompat/function_ref.hpp>

namespace json_query::json_path::detail {

using json_query::json_path::internal::ContainerCursor;
using internal::acquirePooledArray;
using internal::IterativeRecursiveDescent;

// ---------------------------------------------------------------------------
//  Internal implementation helpers
// ---------------------------------------------------------------------------

static std::expected<QJsonArray, EvalError> evaluateWildcardObjectImpl(const QJsonObject& obj)
{
    // Use pooled array to reduce allocations
    auto pooledArray{acquirePooledArray()};
    auto& out = *pooledArray;
    
    // Use ContainerCursor for optimized, zero-copy iteration
    auto cursor{ContainerCursor::object(obj)};
    for (const auto& value : cursor) {
        out.append(value);
    }
    
    // Move result to avoid copying
    auto finalResult = std::move(out);
    return finalResult;
}

// ---------------------------------------------------------------------------
//  Pattern detection for Phase 2 optimizations
// ---------------------------------------------------------------------------

/**
 * @brief Detects common recursive patterns for optimization
 */
struct RecursivePatternDetector {
    // Common patterns that benefit from early termination
    static constexpr std::array<QStringView, 6> EARLY_TERMINATION_PATTERNS = {
        u"title", u"name", u"id", u"type", u"value", u"data"
    };
    
    /**
     * @brief Detect if a JSONPath pattern can benefit from early termination
     * @param pathExpression The JSONPath expression (e.g., "$..title")
     * @return Target field name if early termination applicable, empty otherwise
     */
    static QStringView detectEarlyTerminationPattern(QStringView pathExpression) {
        // Look for patterns like "$..fieldname" 
        if (pathExpression.startsWith(u"$..") && pathExpression.size() > 3) {
            auto fieldName = pathExpression.mid(3);
            
            // Check if it's a simple field access (no additional operators)
            if (!fieldName.contains(u'[') && !fieldName.contains(u'.') && 
                !fieldName.contains(u'?') && !fieldName.contains(u'*')) {
                
                // Check if it matches common patterns
                for (const auto& pattern : EARLY_TERMINATION_PATTERNS) {
                    if (fieldName == pattern) {
                        return fieldName;
                    }
                }
            }
        }
        return {};
    }
    
    /**
     * @brief Estimate document complexity for optimization selection
     */
    static size_t estimateDocumentComplexity(const QJsonValue& value) {
        if (value.isObject()) {
            const auto obj = value.toObject();
            auto complexity = obj.size();
            
            // Sample a few values to estimate nesting
            auto samples = 0;
            for (auto it = obj.begin(); it != obj.end() && samples < 3; ++it, ++samples) {
                if (it.value().isObject() || it.value().isArray()) {
                    complexity += estimateDocumentComplexity(it.value()) / 2;
                }
            }
            return complexity;
        } else if (value.isArray()) {
            const auto arr = value.toArray();
            auto complexity = arr.size();
            
            // Sample first few elements
            for (qsizetype i = 0; i < std::min(arr.size(), qsizetype(3)); ++i) {
                if (arr[i].isObject() || arr[i].isArray()) {
                    complexity += estimateDocumentComplexity(arr[i]) / 2;
                }
            }
            return complexity;
        }
        return 1;
    }
};

static std::expected<QJsonArray, EvalError> evaluateRecursiveImpl(const QJsonValue& value)
{
    // Use iterative implementation with array pooling for better memory efficiency
    return IterativeRecursiveDescent::evaluateIterativeArray(value);
}

/**
 * @brief Phase 3+: Direct recursive fast path bypassing JSONPath evaluation overhead
 * 
 * For simple patterns like "$..fieldname", this bypasses the entire JSONPath
 * evaluation machinery and implements a direct recursive traversal that matches
 * the performance characteristics of plain recursive code.
 * 
 * Uses stdcompat::function_ref for zero-allocation recursive calls.
 */
static std::expected<QJsonArray, EvalError> evaluateRecursiveDirectFastPath(
    const QJsonValue& root,
    QStringView targetField) {
    
    // Use pooled array for results
    auto pooledArray{acquirePooledArray()};
    auto& results = *pooledArray;
    
    // Direct recursive traversal using function_ref - fixed approach
    // We use a struct with operator() to avoid circular lambda capture issues
    struct RecursiveTraverser {
        QJsonArray& results;
        QStringView targetField;
        
        void operator()(const QJsonValue& value) const {
            if (value.isObject()) {
                const auto obj = value.toObject();
                
                // Direct key lookup - fastest possible path
                if (auto it = obj.find(QString(targetField)); it != obj.end()) {
                    results.append(it.value());
                }
                
                // Traverse nested objects/arrays with minimal overhead
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    const auto& nested = it.value();
                    if (nested.isObject() || nested.isArray()) {
                        (*this)(nested);  // Direct recursive call
                    }
                }
            }
            else if (value.isArray()) {
                const auto arr = value.toArray();
                for (const QJsonValue& element : arr) {
                    if (element.isObject() || element.isArray()) {
                        (*this)(element);  // Direct recursive call
                    }
                }
            }
        }
    };
    
    // Create traverser and execute
    RecursiveTraverser traverser{results, targetField};
    traverser(root);
    
    // Move result to avoid copying
    auto finalResult = std::move(results);
    return finalResult;
}

static std::expected<QJsonArray, EvalError> evaluateRecursiveOptimized(
    const QJsonValue& value, 
    QStringView pathHint = QStringView()) {
    
    // Detect if we can use early termination optimization
    auto targetField = RecursivePatternDetector::detectEarlyTerminationPattern(pathHint);
    
    // Estimate document complexity to choose optimization strategy
    auto complexity = RecursivePatternDetector::estimateDocumentComplexity(value);
    
    // Phase 3+: Direct recursive fast path for maximum performance
    if (!targetField.isEmpty() && complexity > 50) {
        return evaluateRecursiveDirectFastPath(value, targetField);
    }
    
    // Phase 3: Use advanced optimizations for simple patterns on complex documents
    if (!targetField.isEmpty() && complexity > 200) {
        auto pooledArray{acquirePooledArray()};
        internal::ResultCollector collector(pooledArray.get());
        
        auto result = IterativeRecursiveDescent::evaluateIterativePhase3Optimized(
            value, targetField, collector);
        
        if (result) {
            // Move result to avoid copying
            auto finalResult = std::move(*pooledArray);
            return finalResult;
        }
    }
    
    // Phase 2: Early termination for moderately complex documents
    if (!targetField.isEmpty() && complexity > 25) {
        auto pooledArray{acquirePooledArray()};
        internal::ResultCollector collector(pooledArray.get());
        
        auto result = IterativeRecursiveDescent::evaluateIterativeWithEarlyTermination(
            value, targetField, collector);
        
        if (!result) {
            return std::unexpected(result.error());
        }
        
        // Move result to avoid copying
        auto finalResult = std::move(*pooledArray);
        return finalResult;
    }
    
    // Fallback to standard implementation
    return IterativeRecursiveDescent::evaluateIterativeArray(value);
}

// ---------------------------------------------------------------------------
//  Public wildcard evaluation API
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> wildcardObject(const QJsonObject& obj)
{
    return evaluateWildcardObjectImpl(obj);
}

std::expected<QJsonArray, EvalError> wildcardArray(const QJsonArray& arr)
{
    // Use pooled array to reduce allocations
    auto pooledArray{acquirePooledArray()};
    auto& out = *pooledArray;
    
    // Use ContainerCursor for optimized, zero-copy iteration
    auto cursor{ContainerCursor::array(arr)};
    for (const auto& item : cursor) {
        out.append(item);
    }
    
    // Move result to avoid copying
    auto finalResult = std::move(out);
    return finalResult;
}

// ---------------------------------------------------------------------------
//  Public recursive evaluation API
// ---------------------------------------------------------------------------

std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, QStringView pathHint)
{
    return evaluateRecursiveOptimized(value, pathHint);
}

std::expected<QJsonArray, EvalError> evaluateRecursive(const QJsonValue& value, int /*unused*/)
{
    return evaluateRecursiveImpl(value);
}

} // namespace json_query::json_path::detail
