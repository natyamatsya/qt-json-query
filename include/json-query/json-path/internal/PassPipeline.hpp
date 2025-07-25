#pragma once

#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathEvalError.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <QJsonObject>
#include <expected>
#include <vector>
#include <memory>
#include <string_view>
#include <functional>
#include <map>
#include <set>
#include <any>
#include <optional>
#include <chrono>

namespace json_query::json_path::internal {

// Forward declarations
class PassManager;
class PassContext;

/**
 * @brief LLVM-inspired pass pipeline architecture for JSONPath evaluation
 * 
 * This system provides a modular, extensible framework for applying optimization
 * and transformation passes to JSONPath evaluation, similar to LLVM's pass manager.
 */

/**
 * @brief Base class for all evaluation passes
 * 
 * Inspired by LLVM's Pass class hierarchy, this provides the interface
 * for transformation and analysis passes in the JSONPath evaluation pipeline.
 */
class Pass {
public:
    enum class Kind {
        Analysis,       // Collects information without modifying the pipeline
        Transform,      // Modifies the evaluation pipeline or context
        Optimization    // Applies performance optimizations
    };

    virtual ~Pass() = default;

    /**
     * @brief Get the pass name for debugging and introspection
     */
    virtual std::string_view getName() const = 0;

    /**
     * @brief Get the pass kind for scheduling and dependency analysis
     */
    virtual Kind getKind() const = 0;

    /**
     * @brief Get pass dependencies (passes that must run before this one)
     */
    virtual std::vector<std::string_view> getDependencies() const { return {}; }

    /**
     * @brief Check if this pass should run given the current context
     */
    virtual bool shouldRun(const PassContext& context) const { return true; }

    /**
     * @brief Run the pass on the evaluation context
     * @return true if the pass made changes, false otherwise
     */
    virtual bool runPass(PassContext& context) = 0;

protected:
    Pass() = default;
};

/**
 * @brief Context passed to passes containing evaluation state and metadata
 * 
 * Similar to LLVM's pass context, this provides passes with access to
 * the evaluation context and allows them to communicate through metadata.
 */
class PassContext {
public:
    using PathEvalCtx = detail::PathEvalCtx;

    explicit PassContext(const PathEvalCtx& evalCtx, const QJsonValue& root)
        : evalContext(evalCtx), rootValue(root) {}

    // Access to evaluation context
    const PathEvalCtx& getEvalContext() const { return evalContext; }
    const QJsonValue& getRootValue() const { return rootValue; }

    // Pass metadata and communication
    template<typename T>
    void setMetadata(std::string_view key, T&& value) {
        metadata[std::string(key)] = std::forward<T>(value);
    }

    template<typename T>
    std::optional<T> getMetadata(std::string_view key) const {
        auto it = metadata.find(std::string(key));
        if (it != metadata.end()) {
            if (auto* ptr = std::any_cast<T>(&it->second)) {
                return *ptr;
            }
        }
        return std::nullopt;
    }

    // Optimization hints and flags
    void setOptimizationLevel(int level) { optimizationLevel = level; }
    int getOptimizationLevel() const { return optimizationLevel; }

    void enableOptimization(std::string_view name) { enabledOptimizations.insert(std::string(name)); }
    void disableOptimization(std::string_view name) { enabledOptimizations.erase(std::string(name)); }
    bool isOptimizationEnabled(std::string_view name) const {
        return enabledOptimizations.count(std::string(name)) > 0;
    }

    // Statistics and profiling
    void incrementCounter(std::string_view name) { counters[std::string(name)]++; }
    size_t getCounter(std::string_view name) const {
        auto it = counters.find(std::string(name));
        return it != counters.end() ? it->second : 0;
    }

private:
    const PathEvalCtx& evalContext;
    const QJsonValue& rootValue;
    std::map<std::string, std::any> metadata;
    std::set<std::string> enabledOptimizations;
    std::map<std::string, size_t> counters;
    int optimizationLevel = 0;
};

/**
 * @brief LLVM-inspired pass manager for orchestrating evaluation passes
 * 
 * Manages pass scheduling, dependency resolution, and execution order.
 * Provides different optimization levels similar to LLVM's -O0, -O1, -O2, -O3.
 */
class PassManager {
public:
    enum class OptimizationLevel {
        O0,  // No optimizations - debug mode
        O1,  // Basic optimizations - size optimizations
        O2,  // Standard optimizations - balanced performance/compile time
        O3   // Aggressive optimizations - maximum performance
    };

    PassManager() = default;

    /**
     * @brief Add a pass to the pipeline
     */
    void addPass(std::unique_ptr<Pass> pass);

    /**
     * @brief Set optimization level and configure appropriate passes
     */
    void setOptimizationLevel(OptimizationLevel level);

    /**
     * @brief Run all passes in the pipeline
     * @return true if any pass made changes
     */
    bool runPasses(PassContext& context);

    /**
     * @brief Get statistics about pass execution
     */
    struct PassStats {
        std::string passName;
        bool didRun;
        bool madeChanges;
        std::chrono::microseconds executionTime;
    };
    
    const std::vector<PassStats>& getPassStats() const { return passStats; }

    /**
     * @brief Clear all passes and statistics
     */
    void clear();

    /**
     * @brief Create a default pipeline for the given optimization level
     */
    static std::unique_ptr<PassManager> createDefaultPipeline(OptimizationLevel level);

private:
    std::vector<std::unique_ptr<Pass>> passes;
    std::vector<PassStats> passStats;
    OptimizationLevel currentOptLevel = OptimizationLevel::O2;

    /**
     * @brief Resolve pass dependencies and determine execution order
     */
    std::vector<size_t> resolveDependencies() const;
};

/**
 * @brief Analysis pass that collects token usage statistics
 */
class TokenAnalysisPass : public Pass {
public:
    std::string_view getName() const override { return "token-analysis"; }
    Kind getKind() const override { return Kind::Analysis; }
    bool runPass(PassContext& context) override;
};

/**
 * @brief Optimization pass that detects and optimizes consecutive wildcard tokens
 */
class WildcardCoalescingPass : public Pass {
public:
    std::string_view getName() const override { return "wildcard-coalescing"; }
    Kind getKind() const override { return Kind::Optimization; }
    std::vector<std::string_view> getDependencies() const override { return {"token-analysis"}; }
    bool shouldRun(const PassContext& context) const override;
    bool runPass(PassContext& context) override;
};

/**
 * @brief Optimization pass that pre-computes filter expressions
 */
class FilterPrecomputationPass : public Pass {
public:
    std::string_view getName() const override { return "filter-precomputation"; }
    Kind getKind() const override { return Kind::Optimization; }
    bool shouldRun(const PassContext& context) const override;
    bool runPass(PassContext& context) override;
};

/**
 * @brief Transform pass that reorders tokens for optimal evaluation
 */
class TokenReorderingPass : public Pass {
public:
    std::string_view getName() const override { return "token-reordering"; }
    Kind getKind() const override { return Kind::Transform; }
    std::vector<std::string_view> getDependencies() const override { return {"token-analysis"}; }
    bool shouldRun(const PassContext& context) const override;
    bool runPass(PassContext& context) override;
};

/**
 * @brief Analysis pass that estimates evaluation complexity
 */
class ComplexityAnalysisPass : public Pass {
public:
    std::string_view getName() const override { return "complexity-analysis"; }
    Kind getKind() const override { return Kind::Analysis; }
    bool runPass(PassContext& context) override;
};

/**
 * @brief Optimization pass that enables streaming evaluation for large datasets
 */
class StreamingOptimizationPass : public Pass {
public:
    std::string_view getName() const override { return "streaming-optimization"; }
    Kind getKind() const override { return Kind::Optimization; }
    std::vector<std::string_view> getDependencies() const override { return {"complexity-analysis"}; }
    bool shouldRun(const PassContext& context) const override;
    bool runPass(PassContext& context) override;
};

} // namespace json_query::json_path::internal
