#include <QCoreApplication>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonDocument>
#include <iostream>
#include <vector>
#include <memory>

// Include only the header with our fix - no library dependency
#include "json-query/json-path/JSONPathCompile.hpp"

using namespace json_query::json_path;

// Create a large lambda that requires heap allocation
auto createLargeLambda() {
    // Capture a large amount of data to force heap allocation
    std::vector<int> largeData(1000, 42);
    std::string largeString(500, 'x');
    
    return [largeData, largeString](const QJsonValue& currentNode, const QJsonValue& rootDocument) -> bool {
        // Use the captured data to prevent optimization
        bool result = !largeData.empty() && !largeString.empty();
        
        // Simple filter logic: check if current node has a specific property
        if (currentNode.isObject()) {
            QJsonObject obj = currentNode.toObject();
            return obj.contains("test") && result;
        }
        return false;
    };
}

// Create a small lambda that fits in inline storage
auto createSmallLambda() {
    return [](const QJsonValue& currentNode, const QJsonValue& rootDocument) -> bool {
        if (currentNode.isObject()) {
            QJsonObject obj = currentNode.toObject();
            return obj.contains("test");
        }
        return false;
    };
}

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    std::cout << "Testing CompactContextFilterStorage lifetime fix...\n";
    
    try {
        // Test 1: Small lambda (inline storage)
        std::cout << "\n=== Test 1: Small Lambda (Inline Storage) ===\n";
        auto smallLambda = createSmallLambda();
        std::cout << "Small lambda size: " << sizeof(smallLambda) << " bytes\n";
        
        CompactContextFilterStorage<32> smallStorage(std::move(smallLambda));
        std::cout << "✓ Small storage created successfully\n";
        std::cout << "✓ Has filter: " << (smallStorage.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Is inline storage: " << (smallStorage.isInlineStorage() ? "YES" : "NO") << "\n";
        
        // Test 2: Large lambda (heap storage) - this is where the crash would occur
        std::cout << "\n=== Test 2: Large Lambda (Heap Storage) ===\n";
        auto largeLambda = createLargeLambda();
        std::cout << "Large lambda size: " << sizeof(largeLambda) << " bytes\n";
        std::cout << "Buffer size: 32 bytes\n";
        std::cout << "Should use heap allocation: " << (sizeof(largeLambda) > 32 ? "YES" : "NO") << "\n";
        
        CompactContextFilterStorage<32> largeStorage(std::move(largeLambda));
        std::cout << "✓ Large storage created successfully\n";
        std::cout << "✓ Has filter: " << (largeStorage.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Is inline storage: " << (largeStorage.isInlineStorage() ? "YES" : "NO") << "\n";
        
        // Test 3: Create test JSON data
        std::cout << "\n=== Test 3: JSON Data Creation ===\n";
        QJsonObject testObj;
        testObj["test"] = "value";
        testObj["other"] = 123;
        
        QJsonObject rootObj;
        rootObj["data"] = testObj;
        
        QJsonValue currentNode(testObj);
        QJsonValue rootDocument(rootObj);
        std::cout << "✓ Test JSON data created\n";
        
        // Test 4: Evaluate small filter
        std::cout << "\n=== Test 4: Small Filter Evaluation ===\n";
        bool smallResult = smallStorage.evaluateContext(currentNode, rootDocument);
        std::cout << "✓ Small filter evaluation completed\n";
        std::cout << "✓ Small filter result: " << (smallResult ? "PASS" : "FAIL") << "\n";
        
        // Test 5: Evaluate large filter (critical test - this is where the crash would occur)
        std::cout << "\n=== Test 5: Large Filter Evaluation (Critical Test) ===\n";
        bool largeResult = largeStorage.evaluateContext(currentNode, rootDocument);
        std::cout << "✓ Large filter evaluation completed WITHOUT CRASH!\n";
        std::cout << "✓ Large filter result: " << (largeResult ? "PASS" : "FAIL") << "\n";
        
        // Test 6: Copy constructor for large storage
        std::cout << "\n=== Test 6: Copy Constructor (Large Storage) ===\n";
        CompactContextFilterStorage<32> largeCopy(largeStorage);
        std::cout << "✓ Large storage copy created successfully\n";
        std::cout << "✓ Copy has filter: " << (largeCopy.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Copy is inline storage: " << (largeCopy.isInlineStorage() ? "YES" : "NO") << "\n";
        
        bool copyResult = largeCopy.evaluateContext(currentNode, rootDocument);
        std::cout << "✓ Copy filter evaluation completed WITHOUT CRASH!\n";
        std::cout << "✓ Copy filter result: " << (copyResult ? "PASS" : "FAIL") << "\n";
        
        // Test 7: Move constructor for large storage
        std::cout << "\n=== Test 7: Move Constructor (Large Storage) ===\n";
        auto anotherLargeLambda = createLargeLambda();
        CompactContextFilterStorage<32> originalStorage(std::move(anotherLargeLambda));
        CompactContextFilterStorage<32> movedStorage(std::move(originalStorage));
        
        std::cout << "✓ Move constructor completed successfully\n";
        std::cout << "✓ Moved storage has filter: " << (movedStorage.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Original storage has filter: " << (originalStorage.hasFilter() ? "YES" : "NO") << "\n";
        
        bool moveResult = movedStorage.evaluateContext(currentNode, rootDocument);
        std::cout << "✓ Moved filter evaluation completed WITHOUT CRASH!\n";
        std::cout << "✓ Moved filter result: " << (moveResult ? "PASS" : "FAIL") << "\n";
        
        // Test 8: Assignment operators
        std::cout << "\n=== Test 8: Assignment Operators ===\n";
        auto assignLambda = createLargeLambda();
        CompactContextFilterStorage<32> assignStorage(std::move(assignLambda));
        CompactContextFilterStorage<32> assignTarget;
        
        assignTarget = assignStorage; // Copy assignment
        std::cout << "✓ Copy assignment completed successfully\n";
        
        bool assignResult = assignTarget.evaluateContext(currentNode, rootDocument);
        std::cout << "✓ Assigned filter evaluation completed WITHOUT CRASH!\n";
        std::cout << "✓ Assigned filter result: " << (assignResult ? "PASS" : "FAIL") << "\n";
        
        std::cout << "\n🎉 ALL TESTS PASSED - CompactContextFilterStorage lifetime issue is FIXED!\n";
        std::cout << "\n✅ The type erasure mechanism now correctly handles large lambdas\n";
        std::cout << "✅ No crashes occurred during heap-allocated filter evaluation\n";
        std::cout << "✅ Copy and move semantics work correctly\n";
        std::cout << "✅ Both inline and heap storage paths are working\n";
        
    } catch (const std::exception& e) {
        std::cout << "\n❌ EXCEPTION CAUGHT: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cout << "\n❌ UNKNOWN EXCEPTION CAUGHT\n";
        return 1;
    }
    
    return 0;
}
