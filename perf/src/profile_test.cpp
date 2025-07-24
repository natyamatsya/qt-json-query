#include <chrono>
#include <iostream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <gperftools/profiler.h>
#include "json-query/json-path/JSONPath.hpp"

using namespace json_query;
using namespace std::chrono;

// Create test document similar to benchmark
QJsonObject createTestDocument() {
    QJsonObject root;
    root["name"] = "TestDocument";
    
    QJsonObject address;
    address["street"] = "123 Main St";
    address["city"] = "TestCity";
    root["address"] = address;
    
    QJsonArray inventory;
    for (int i = 0; i < 50; ++i) {
        QJsonObject item;
        item["id"] = i;
        item["name"] = QString("Item %1").arg(i);
        item["price"] = 10.0 + (i % 20);
        
        QJsonArray categories;
        categories.append("category1");
        categories.append("category2");
        item["categories"] = categories;
        
        inventory.append(item);
    }
    root["inventory"] = inventory;
    
    return root;
}

int main() {
    QJsonObject testDoc = createTestDocument();
    
    // Test cases that showed high overhead
    std::vector<std::pair<std::string, std::string>> testCases = {
        {"Simple", "$.name"},
        {"Nested", "$.address.city"},
        {"Array", "$.inventory[25].categories[1]"},
        {"Filter", "$.inventory[?(@.price > 20)]"},
        {"Recursive", "$..name"}
    };
    
    const int iterations = 10000;
    
    for (const auto& [name, path] : testCases) {
        std::cout << "\n=== Profiling: " << name << " (" << path << ") ===" << std::endl;
        
        // Compile once
        auto jsonPath = JSONPath::create(QString::fromStdString(path));
        if (!jsonPath) {
            std::cout << "Failed to compile path: " << path << std::endl;
            continue;
        }
        
        // Profile evaluation
        auto start = high_resolution_clock::now();
        
        ProfilerStart("profile.prof");
        
        for (int i = 0; i < iterations; ++i) {
            auto result = jsonPath->evaluate(testDoc);
            // Prevent optimization
            volatile auto dummy = result.has_value();
            (void)dummy;
        }
        
        ProfilerStop();
        
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<nanoseconds>(end - start).count();
        
        std::cout << "Total time: " << duration << " ns" << std::endl;
        std::cout << "Average per iteration: " << (duration / iterations) << " ns" << std::endl;
        std::cout << "Iterations: " << iterations << std::endl;
    }
    
    return 0;
}
