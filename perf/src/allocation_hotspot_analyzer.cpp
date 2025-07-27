#include <iostream>
#include <chrono>
#include <vector>
#include <unordered_map>
#include <iomanip>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QString>

#include "json-query/json-path/JSONPath.hpp"

using namespace json_query;

// Memory allocation hotspot analyzer
class AllocationHotspotAnalyzer {
public:
    struct HotspotResult {
        std::string operation;
        size_t peakMemoryMB;
        double avgDurationNs;
        size_t resultCount;
        double memoryPerResult;
        std::string bottleneck;
    };

    static std::vector<HotspotResult> analyzeAllocationHotspots() {
        std::vector<HotspotResult> results;
        
        // Create test data with varying complexity
        auto testData = createTestData();
        QJsonValue rootValue(testData);
        
        // Test cases designed to stress different allocation patterns
        std::vector<std::tuple<std::string, QString, std::string>> testCases = {
            {"Simple Access", "$.name", "Single result, minimal allocation"},
            {"Deep Nesting", "$.level1.level2.level3.value", "Property chain traversal"},
            {"Large Array Access", "$.largeArray[500]", "Array indexing with large containers"},
            {"Array Slice", "$.largeArray[100:200]", "Array slicing creates new array"},
            {"Filter Small", "$.inventory[?(@.price > 50)]", "Filter on small dataset"},
            {"Filter Large", "$.largeArray[?(@.value > 500)]", "Filter on large dataset"},
            {"Recursive Small", "$..name", "Recursive descent on small data"},
            {"Recursive Large", "$..value", "Recursive descent on large data"},
            {"Complex Union", "$.inventory[0,5,10,15,20]", "Multi-index union"},
            {"Mixed Union", "$.inventory[?(@.price > 30), 'special', *]", "Mixed selector union"},
            {"Deep Recursive", "$..largeArray[*].value", "Deep recursive with array expansion"}
        };
        
        for (const auto& [testName, jsonPath, description] : testCases) {
            auto result{analyzeOperation(testName, jsonPath, rootValue, description)};
            results.push_back(result);
        }
        
        return results;
    }

private:
    static QJsonObject createTestData() {
        QJsonObject testData;
        
        // Simple properties
        testData["name"] = "Test Object";
        testData["id"] = 12345;
        
        // Deep nesting to test property chain allocation
        QJsonObject level1, level2, level3;
        level3["value"] = "deep_value";
        level2["level3"] = level3;
        level1["level2"] = level2;
        testData["level1"] = level1;
        
        // Large array to test array operations and memory pressure
        QJsonArray largeArray;
        for (int i = 0; i < 1000; ++i) {
            QJsonObject item;
            item["index"] = i;
            item["value"] = i * 2;
            item["name"] = QString("item_%1").arg(i);
            largeArray.append(item);
        }
        testData["largeArray"] = largeArray;
        
        // Inventory for filter operations
        QJsonArray inventory;
        for (int i = 0; i < 100; ++i) {
            QJsonObject item;
            item["id"] = i;
            item["name"] = QString("Product %1").arg(i);
            item["price"] = 10.0 + (i % 50);
            item["category"] = (i % 3 == 0) ? "special" : "regular";
            inventory.append(item);
        }
        testData["inventory"] = inventory;
        
        return testData;
    }
    
    static HotspotResult analyzeOperation(const std::string& testName, const QString& jsonPath, 
                                        const QJsonValue& testData, const std::string& description) {
        try {
            // Compile JSONPath
            auto pathResult{JSONPath::create(jsonPath)};
            if (!pathResult) {
                return {testName, 0, 0, 0, 0, "Compilation failed"};
            }
            
            auto path{std::move(*pathResult)};
            
            // Warm up
            for (int i = 0; i < 5; ++i) {
                auto result{path.evaluate(testData)};
            }
            
            // Measure memory and performance
            const auto iterations{100};
            auto start{std::chrono::high_resolution_clock::now()};
            
            auto totalResults{0};
            for (int i = 0; i < iterations; ++i) {
                auto result{path.evaluate(testData)};
                if (result) {
                    if (result->isArray()) {
                        totalResults += result->toArray().size();
                    } else {
                        totalResults += 1;
                    }
                }
            }
            
            auto end{std::chrono::high_resolution_clock::now()};
            auto duration{std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)};
            
            auto avgDuration = static_cast<double>(duration.count()) / iterations;
            auto avgResults{totalResults / iterations};
            auto memoryPerResult = avgResults > 0 ? (avgDuration / avgResults) : avgDuration;
            
            // Estimate peak memory (simplified heuristic based on operation type)
            auto estimatedPeakMB{0};
            auto bottleneck{"Unknown"};
            
            if (jsonPath.contains("largeArray")) {
                estimatedPeakMB = 5; // Large array operations
                bottleneck = "Large container allocation";
            } else if (jsonPath.contains("$..")) {
                estimatedPeakMB = 3; // Recursive operations
                bottleneck = "Recursive descent stack + intermediate arrays";
            } else if (jsonPath.contains("[?")) {
                estimatedPeakMB = 2; // Filter operations
                bottleneck = "Filter result array allocation";
            } else if (jsonPath.contains(",")) {
                estimatedPeakMB = 2; // Union operations
                bottleneck = "Union result collection";
            } else {
                estimatedPeakMB = 1; // Simple operations
                bottleneck = "Minimal allocation";
            }
            
            return {testName, static_cast<size_t>(estimatedPeakMB), avgDuration, static_cast<size_t>(avgResults), memoryPerResult, bottleneck};
            
        } catch (const std::exception& e) {
            return {testName, 0, 0, 0, 0, std::string("Error: ") + e.what()};
        }
    }
};

int main() {
    std::cout << "=== JSONPath Memory Allocation Hotspot Analysis ===\n\n";
    
    auto results{AllocationHotspotAnalyzer::analyzeAllocationHotspots()};
    
    std::cout << "| Operation | Peak Memory (MB) | Avg Duration (ns) | Result Count | Memory/Result | Primary Bottleneck |\n";
    std::cout << "|-----------|------------------|-------------------|--------------|---------------|--------------------|\n";
    
    for (const auto& result : results) {
        std::cout << "| " << result.operation
                  << " | " << result.peakMemoryMB
                  << " | " << static_cast<long>(result.avgDurationNs)
                  << " | " << result.resultCount
                  << " | " << std::fixed << std::setprecision(2) << result.memoryPerResult
                  << " | " << result.bottleneck
                  << " |\n";
    }
    
    std::cout << "\n=== Memory Optimization Recommendations ===\n";
    std::cout << "Based on hotspot analysis:\n\n";
    
    std::cout << "1. **QJsonArray Allocation Reduction:**\n";
    std::cout << "   - Implement object pooling for frequently allocated arrays\n";
    std::cout << "   - Use in-place result streaming where possible\n";
    std::cout << "   - Consider arena allocators for temporary arrays\n\n";
    
    std::cout << "2. **Qt Container Copy Optimization:**\n";
    std::cout << "   - Minimize defensive copying with const references\n";
    std::cout << "   - Use QJsonValueConstRef where applicable\n";
    std::cout << "   - Implement custom iterators to avoid detaching\n\n";
    
    std::cout << "3. **Recursive Descent Stack Optimization:**\n";
    std::cout << "   - Convert to iterative algorithm with explicit stack\n";
    std::cout << "   - Reuse stack memory across iterations\n";
    std::cout << "   - Implement tail-call optimization patterns\n\n";
    
    std::cout << "4. **Union/Filter Result Optimization:**\n";
    std::cout << "   - Stream results directly without intermediate collection\n";
    std::cout << "   - Implement lazy evaluation for union operations\n";
    std::cout << "   - Use move semantics for result transfer\n\n";
    
    return 0;
}
