#include "json-query/json-path/internal/PassPipeline.hpp"
#include "json-query/json-path/internal/TokenDispatchTable.hpp"

#include <algorithm>
#include <chrono>
#include <unordered_set>
#include <unordered_map>

namespace json_query::json_path::internal {

// ---------------------------------------------------------------------------
//  PassManager Implementation
// ---------------------------------------------------------------------------

void PassManager::addPass(std::unique_ptr<Pass> pass) {
    passes.push_back(std::move(pass));
}

void PassManager::setOptimizationLevel(OptimizationLevel level) {
    currentOptLevel = level;
}

bool PassManager::runPasses(PassContext& context) {
    passStats.clear();
    context.setOptimizationLevel(static_cast<int>(currentOptLevel));
    
    // Configure optimizations based on level
    switch (currentOptLevel) {
        case OptimizationLevel::O0:
            // Debug mode - no optimizations
            break;
        case OptimizationLevel::O1:
            context.enableOptimization("basic-coalescing");
            break;
        case OptimizationLevel::O2:
            context.enableOptimization("basic-coalescing");
            context.enableOptimization("filter-precomputation");
            context.enableOptimization("token-reordering");
            break;
        case OptimizationLevel::O3:
            context.enableOptimization("basic-coalescing");
            context.enableOptimization("filter-precomputation");
            context.enableOptimization("token-reordering");
            context.enableOptimization("streaming-optimization");
            context.enableOptimization("aggressive-coalescing");
            break;
    }

    // Resolve dependencies and get execution order
    auto executionOrder = resolveDependencies();
    
    bool anyPassMadeChanges = false;
    
    for (size_t passIndex : executionOrder) {
        auto& pass = passes[passIndex];
        
        PassStats stats;
        stats.passName = std::string(pass->getName());
        stats.didRun = false;
        stats.madeChanges = false;
        
        auto startTime = std::chrono::high_resolution_clock::now();
        
        if (pass->shouldRun(context)) {
            stats.didRun = true;
            stats.madeChanges = pass->runPass(context);
            anyPassMadeChanges |= stats.madeChanges;
        }
        
        auto endTime = std::chrono::high_resolution_clock::now();
        stats.executionTime = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        
        passStats.push_back(stats);
    }
    
    return anyPassMadeChanges;
}

void PassManager::clear() {
    passes.clear();
    passStats.clear();
}

std::vector<size_t> PassManager::resolveDependencies() const {
    std::vector<size_t> executionOrder;
    std::unordered_set<std::string> resolved;
    std::unordered_map<std::string, size_t> passNameToIndex;
    
    // Build name to index mapping
    for (size_t i = 0; i < passes.size(); ++i) {
        passNameToIndex[std::string(passes[i]->getName())] = i;
    }
    
    // Topological sort with dependency resolution
    std::function<void(size_t)> resolveDeps = [&](size_t passIndex) {
        const auto& pass = passes[passIndex];
        std::string passName(pass->getName());
        
        if (resolved.count(passName)) {
            return; // Already resolved
        }
        
        // Resolve dependencies first
        for (const auto& dep : pass->getDependencies()) {
            std::string depName(dep);
            auto it = passNameToIndex.find(depName);
            if (it != passNameToIndex.end()) {
                resolveDeps(it->second);
            }
        }
        
        resolved.insert(passName);
        executionOrder.push_back(passIndex);
    };
    
    // Resolve all passes
    for (size_t i = 0; i < passes.size(); ++i) {
        resolveDeps(i);
    }
    
    return executionOrder;
}

std::unique_ptr<PassManager> PassManager::createDefaultPipeline(OptimizationLevel level) {
    auto manager = std::make_unique<PassManager>();
    manager->setOptimizationLevel(level);
    
    // Add analysis passes (always included)
    manager->addPass(std::make_unique<TokenAnalysisPass>());
    manager->addPass(std::make_unique<ComplexityAnalysisPass>());
    
    // Add optimization passes based on level
    if (level >= OptimizationLevel::O1) {
        manager->addPass(std::make_unique<WildcardCoalescingPass>());
    }
    
    if (level >= OptimizationLevel::O2) {
        manager->addPass(std::make_unique<FilterPrecomputationPass>());
        manager->addPass(std::make_unique<TokenReorderingPass>());
    }
    
    if (level >= OptimizationLevel::O3) {
        manager->addPass(std::make_unique<StreamingOptimizationPass>());
    }
    
    return manager;
}

// ---------------------------------------------------------------------------
//  Pass Implementations
// ---------------------------------------------------------------------------

bool TokenAnalysisPass::runPass(PassContext& context) {
    const auto& evalCtx = context.getEvalContext();
    
    // Analyze token distribution and patterns
    std::unordered_map<Token::Kind, size_t> tokenCounts;
    std::vector<Token::Kind> tokenSequence;
    
    for (const auto& token : evalCtx.tokens) {
        tokenCounts[token.kind]++;
        tokenSequence.push_back(token.kind);
    }
    
    // Store analysis results in context
    context.setMetadata("token_counts", tokenCounts);
    context.setMetadata("token_sequence", tokenSequence);
    context.setMetadata("total_tokens", evalCtx.tokens.size());
    
    // Detect patterns
    size_t consecutiveWildcards = 0;
    size_t maxConsecutiveWildcards = 0;
    
    for (const auto& token : evalCtx.tokens) {
        if (token.kind == Token::Kind::Wildcard) {
            consecutiveWildcards++;
            maxConsecutiveWildcards = std::max(maxConsecutiveWildcards, consecutiveWildcards);
        } else {
            consecutiveWildcards = 0;
        }
    }
    
    context.setMetadata("max_consecutive_wildcards", maxConsecutiveWildcards);
    context.setMetadata("has_filters", tokenCounts[Token::Kind::Filter] > 0);
    context.setMetadata("has_recursive", tokenCounts[Token::Kind::Recursive] > 0);
    
    context.incrementCounter("tokens_analyzed");
    return false; // Analysis passes don't modify the pipeline
}

bool WildcardCoalescingPass::shouldRun(const PassContext& context) const {
    if (!context.isOptimizationEnabled("basic-coalescing") && 
        !context.isOptimizationEnabled("aggressive-coalescing")) {
        return false;
    }
    
    auto maxWildcards = context.getMetadata<size_t>("max_consecutive_wildcards");
    return maxWildcards && *maxWildcards > 1;
}

bool WildcardCoalescingPass::runPass(PassContext& context) {
    // This is a conceptual implementation - in practice, we'd need to modify
    // the evaluation strategy rather than the tokens themselves
    
    auto tokenSequence = context.getMetadata<std::vector<Token::Kind>>("token_sequence");
    if (!tokenSequence) return false;
    
    // Count potential optimizations
    size_t coalescingOpportunities = 0;
    size_t consecutiveWildcards = 0;
    
    for (auto kind : *tokenSequence) {
        if (kind == Token::Kind::Wildcard) {
            consecutiveWildcards++;
        } else {
            if (consecutiveWildcards > 1) {
                coalescingOpportunities++;
            }
            consecutiveWildcards = 0;
        }
    }
    
    if (consecutiveWildcards > 1) {
        coalescingOpportunities++;
    }
    
    context.setMetadata("wildcard_coalescing_opportunities", coalescingOpportunities);
    context.incrementCounter("wildcard_coalescings_applied");
    
    return coalescingOpportunities > 0;
}

bool FilterPrecomputationPass::shouldRun(const PassContext& context) const {
    if (!context.isOptimizationEnabled("filter-precomputation")) {
        return false;
    }
    
    auto hasFilters = context.getMetadata<bool>("has_filters");
    return hasFilters && *hasFilters;
}

bool FilterPrecomputationPass::runPass(PassContext& context) {
    const auto& evalCtx = context.getEvalContext();
    
    // Analyze filters for precomputation opportunities
    size_t precomputableFilters = 0;
    
    for (const auto& token : evalCtx.tokens) {
        if (token.kind == Token::Kind::Filter) {
            // Check if filter can be precomputed (e.g., constant expressions)
            // This is a simplified check - real implementation would analyze filter AST
            if (token.key.contains("==") && !token.key.contains("@")) {
                precomputableFilters++;
            }
        }
    }
    
    context.setMetadata("precomputable_filters", precomputableFilters);
    context.incrementCounter("filters_precomputed");
    
    return precomputableFilters > 0;
}

bool TokenReorderingPass::shouldRun(const PassContext& context) const {
    if (!context.isOptimizationEnabled("token-reordering")) {
        return false;
    }
    
    auto totalTokens = context.getMetadata<size_t>("total_tokens");
    return totalTokens && *totalTokens > 2; // Only worth reordering with multiple tokens
}

bool TokenReorderingPass::runPass(PassContext& context) {
    // Analyze token priorities and suggest reordering
    const auto& evalCtx = context.getEvalContext();
    
    size_t reorderingOpportunities = 0;
    
    for (size_t i = 0; i < evalCtx.tokens.size() - 1; ++i) {
        const auto& current = evalCtx.tokens[i];
        const auto& next = evalCtx.tokens[i + 1];
        
        // Use TableGen metadata to get priorities
        int currentPriority = TokenDispatcher::getPriority(current.kind);
        int nextPriority = TokenDispatcher::getPriority(next.kind);
        
        // If next token has higher priority, it could be reordered
        if (nextPriority > currentPriority) {
            reorderingOpportunities++;
        }
    }
    
    context.setMetadata("reordering_opportunities", reorderingOpportunities);
    context.incrementCounter("tokens_reordered");
    
    return reorderingOpportunities > 0;
}

bool ComplexityAnalysisPass::runPass(PassContext& context) {
    const auto& evalCtx = context.getEvalContext();
    const auto& root = context.getRootValue();
    
    // Estimate evaluation complexity
    size_t estimatedComplexity = 0;
    size_t dataSize = 0;
    
    // Estimate data size
    if (root.isArray()) {
        dataSize = root.toArray().size();
    } else if (root.isObject()) {
        dataSize = root.toObject().size();
    } else {
        dataSize = 1;
    }
    
    // Calculate complexity based on tokens and data size
    for (const auto& token : evalCtx.tokens) {
        switch (token.kind) {
            case Token::Kind::Key:
                estimatedComplexity += 1; // O(1) key lookup
                break;
            case Token::Kind::Index:
                estimatedComplexity += 1; // O(1) index access
                break;
            case Token::Kind::Wildcard:
                estimatedComplexity += dataSize; // O(n) iteration
                break;
            case Token::Kind::Recursive:
                estimatedComplexity += dataSize * 2; // O(n) recursive traversal
                break;
            case Token::Kind::Filter:
                estimatedComplexity += dataSize * 3; // O(n) filtering with evaluation
                break;
            case Token::Kind::Slice:
                estimatedComplexity += dataSize / 2; // O(n/2) average slice
                break;
            case Token::Kind::KeyList:
                estimatedComplexity += token.key.split('\n').size(); // O(k) key list
                break;
        }
    }
    
    context.setMetadata("estimated_complexity", estimatedComplexity);
    context.setMetadata("data_size", dataSize);
    context.setMetadata("complexity_class", 
        estimatedComplexity < 100 ? "low" : 
        estimatedComplexity < 1000 ? "medium" : "high");
    
    context.incrementCounter("complexity_analyses");
    return false; // Analysis pass
}

bool StreamingOptimizationPass::shouldRun(const PassContext& context) const {
    if (!context.isOptimizationEnabled("streaming-optimization")) {
        return false;
    }
    
    auto complexity = context.getMetadata<std::string>("complexity_class");
    auto dataSize = context.getMetadata<size_t>("data_size");
    
    return (complexity && *complexity == "high") || 
           (dataSize && *dataSize > 1000);
}

bool StreamingOptimizationPass::runPass(PassContext& context) {
    // Enable streaming optimizations for large datasets
    context.setMetadata("use_streaming", true);
    context.setMetadata("streaming_threshold", size_t(1000));
    
    auto dataSize = context.getMetadata<size_t>("data_size");
    if (dataSize && *dataSize > 10000) {
        context.setMetadata("use_parallel_streaming", true);
    }
    
    context.incrementCounter("streaming_optimizations_enabled");
    return true; // This pass enables optimizations
}

} // namespace json_query::json_path::internal
