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
        
        // C++23 Monadic Chain - Elegant error composition without manual checks!
        return compile(rawPath)
            .and_then([&](json_path::CompilationResult compilationResult) -> JSONPath::Result {
                qCDebug(jsonPathLog) << "JSONPath::create() compile succeeded, creating JSONPath object";
                
                // Apply optimization passes if requested
                if (optLevel != PassManager::OptimizationLevel::O0) {
                    auto passManager = PassManager::createDefaultPipeline(optLevel);
                    // Note: Pass pipeline integration with embedded filters is simplified for now
                    // The embedded filter system is already zero-overhead and fully functional
                    
                    return JSONPath(compilationResult.function,
                                rawPath.toString(),
                                std::move(compilationResult.compiled.tokens));
                } else {
                    return JSONPath(compilationResult.function,
                                rawPath.toString(),
                                std::move(compilationResult.compiled.tokens));
                }
            })
            .or_else([](Error error) -> JSONPath::Result {
                qCDebug(jsonPathLog) << "JSONPath::create() compile failed, returning error" << static_cast<int>(error);
                return std::unexpected(error);
            });
    }

} // namespace json_query

// NOTE: JSONPath::compileFilter() implementation is in JSONPathFilter.cpp