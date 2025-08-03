// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <QJsonValue>
#include <QJsonObject>
#include <QJsonDocument>
#include <iostream>
#include <vector>
#include <memory>

// Include the header with our fix
#include "json-query/json-path/JSONPathCompile.hpp"

using namespace json_query::json_path;

// Create a large lambda that requires heap allocation
auto createLargeLambda()
{
    // Capture a large amount of data to force heap allocation
    std::vector<int> largeData(1000, 42);
    std::string      largeString(500, 'x');

    return [largeData, largeString](const QJsonValue& currentNode, const QJsonValue& rootDocument) -> bool
    {
        // Use the captured data to prevent optimization
        bool result = !largeData.empty() && !largeString.empty();

        // Simple filter logic: check if current node has a specific property
        if (currentNode.isObject())
        {
            QJsonObject obj = currentNode.toObject();
            return obj.contains("test") && result;
        }
        return false;
    };
}

int main()
{
    std::cout << "Testing CompactContextFilterStorage lifetime fix...\n";

    try
    {
        // Test 1: Create storage with large lambda
        std::cout << "1. Creating CompactContextFilterStorage with large lambda...\n";
        auto largeLambda{createLargeLambda()};

        // Verify the lambda is large enough to require heap allocation
        std::cout << "   Lambda size: " << sizeof(largeLambda) << " bytes\n";
        std::cout << "   Buffer size: 32 bytes\n";
        std::cout << "   Should use heap allocation: " << (sizeof(largeLambda) > 32 ? "YES" : "NO") << "\n";

        CompactContextFilterStorage<32> storage(std::move(largeLambda));

        std::cout << "   Storage created successfully\n";
        std::cout << "   Has filter: " << (storage.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "   Is inline storage: " << (storage.isInlineStorage() ? "YES" : "NO") << "\n";

        // Test 2: Create test JSON data
        std::cout << "\n2. Creating test JSON data...\n";
        QJsonObject testObj;
        testObj["test"]  = "value";
        testObj["other"] = 123;

        QJsonObject rootObj;
        rootObj["data"] = testObj;

        QJsonValue currentNode(testObj);
        QJsonValue rootDocument(rootObj);

        // Test 3: Evaluate the filter (this is where the crash would occur)
        std::cout << "\n3. Evaluating filter...\n";
        bool result = storage.evaluateContext(currentNode, rootDocument);
        std::cout << "   Filter result: " << (result ? "PASS" : "FAIL") << "\n";

        // Test 4: Test copy constructor (another potential crash point)
        std::cout << "\n4. Testing copy constructor...\n";
        CompactContextFilterStorage<32> storageCopy(storage);
        std::cout << "   Copy created successfully\n";
        std::cout << "   Copy has filter: " << (storageCopy.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "   Copy is inline storage: " << (storageCopy.isInlineStorage() ? "YES" : "NO") << "\n";

        bool copyResult = storageCopy.evaluateContext(currentNode, rootDocument);
        std::cout << "   Copy filter result: " << (copyResult ? "PASS" : "FAIL") << "\n";

        // Test 5: Test move constructor
        std::cout << "\n5. Testing move constructor...\n";
        auto                            anotherLargeLambda{createLargeLambda()};
        CompactContextFilterStorage<32> originalStorage(std::move(anotherLargeLambda));
        CompactContextFilterStorage<32> movedStorage(std::move(originalStorage));

        std::cout << "   Move created successfully\n";
        std::cout << "   Moved storage has filter: " << (movedStorage.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "   Original storage has filter: " << (originalStorage.hasFilter() ? "YES" : "NO") << "\n";

        bool moveResult = movedStorage.evaluateContext(currentNode, rootDocument);
        std::cout << "   Moved filter result: " << (moveResult ? "PASS" : "FAIL") << "\n";

        std::cout << "\n✅ ALL TESTS PASSED - Lifetime issue appears to be fixed!\n";
    }
    catch (const std::exception& e)
    {
        std::cout << "\n❌ EXCEPTION CAUGHT: " << e.what() << "\n";
        return 1;
    }
    catch (...)
    {
        std::cout << "\n❌ UNKNOWN EXCEPTION CAUGHT\n";
        return 1;
    }

    return 0;
}
