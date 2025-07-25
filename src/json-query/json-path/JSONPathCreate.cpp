#include <json-path/JSONPath.hpp>

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"
#include "json-query/json-path/JSONPathLog.hpp"
#include "json-query/json-path/internal/PassPipeline.hpp"

namespace json_query {
    using json_path::compile;
    using json_path::jsonPathLog;
    using json_path::internal::PassManager;
    using json_path::internal::PassContext;

    JSONPath::Result JSONPath::create(QStringView rawPath)
    {
        // Use O1 as default optimization level for good balance of performance and latency
        return create(rawPath, PassManager::OptimizationLevel::O1);
    }

    // LLVM-inspired compilation with optimization levels
    JSONPath::Result JSONPath::create(QStringView rawPath, PassManager::OptimizationLevel optLevel)
    {
        qCDebug(jsonPathLog) << "JSONPath::create() called with rawPath=" << rawPath << "and optimization level" << static_cast<int>(optLevel);
        
        // Standard compilation first
        auto result = compile(rawPath);
        if (!result.has_value()) {
            qCDebug(jsonPathLog) << "JSONPath::create() compile failed, returning error" << static_cast<int>(result.error());
            return std::unexpected(result.error());
        }
        
        qCDebug(jsonPathLog) << "JSONPath::create() compile succeeded, running optimization passes";
        
        // Create evaluation context for pass pipeline (compilation-time analysis)
        QJsonValue dummyRoot; // Passes analyze structure, not data
        json_path::detail::PathEvalCtx evalCtx(
            result.value().compiled.tokens,
            result.value().compiled.filters, 
            result.value().compiled.contextFilters,
            dummyRoot,
            result.value().function
        );
        
        // Run LLVM-inspired optimization passes during compilation
        PassContext passContext(evalCtx, dummyRoot);
        auto passManager = PassManager::createDefaultPipeline(optLevel);
        
        auto startTime = std::chrono::high_resolution_clock::now();
        bool passesModified = passManager->runPasses(passContext);
        auto endTime = std::chrono::high_resolution_clock::now();
        
        auto passDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
        qCDebug(jsonPathLog) << "Pass pipeline executed in" << passDuration.count() << "μs, modified:" << passesModified;
        
        // TODO: Apply pass optimizations to compiled tokens/filters
        // For now, we use the original compiled result but with pass insights stored
        
        return JSONPath(result.value().function,
                        rawPath.toString(),
                        std::move(result.value().compiled.tokens),
                        std::move(result.value().compiled.filters),
                        std::move(result.value().compiled.contextFilters));
    }

} // namespace json_query

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp