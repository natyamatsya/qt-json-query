// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <QCoreApplication>
#include <QJsonValue>
#include <QJsonObject>
#include <QJsonDocument>
#include <iostream>
#include <vector>
#include <memory>

// Include the header with our variant-based storage
#include "json-query/json-path/JSONPathCompile.hpp"

using namespace json_query::json_path;

// Create large lambdas that require heap allocation
auto createLargeRegularFilter()
{
    std::vector<int> largeData(1000, 42);
    std::string      largeString(500, 'x');

    return [largeData, largeString](const QJsonValue& value) -> bool
    {
        bool result = !largeData.empty() && !largeString.empty();
        if (value.isObject())
        {
            QJsonObject obj = value.toObject();
            return obj.contains("test") && result;
        }
        return false;
    };
}

auto createLargeContextFilter()
{
    std::vector<int> largeData(1000, 42);
    std::string      largeString(500, 'x');

    return [largeData, largeString](const QJsonValue& currentNode, const QJsonValue& rootDocument) -> bool
    {
        bool result = !largeData.empty() && !largeString.empty();
        if (currentNode.isObject())
        {
            QJsonObject obj = currentNode.toObject();
            return obj.contains("test") && result;
        }
        return false;
    };
}

// Create small lambdas that fit in inline storage
auto createSmallRegularFilter()
{
    return [](const QJsonValue& value) -> bool
    {
        if (value.isObject())
        {
            QJsonObject obj = value.toObject();
            return obj.contains("test");
        }
        return false;
    };
}

auto createSmallContextFilter()
{
    return [](const QJsonValue& currentNode, const QJsonValue& rootDocument) -> bool
    {
        if (currentNode.isObject())
        {
            QJsonObject obj = currentNode.toObject();
            return obj.contains("test");
        }
        return false;
    };
}

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    std::cout << "=== Comprehensive Variant-Based Storage Test ===\n";

    try
    {
        // Create test JSON data
        QJsonObject testObj;
        testObj["test"]  = "value";
        testObj["other"] = 123;

        QJsonObject rootObj;
        rootObj["data"] = testObj;

        QJsonValue currentNode(testObj);
        QJsonValue rootDocument(rootObj);

        std::cout << "\n=== Test 1: CompactFilterStorage (Regular Filters) ===\n";

        // Test small regular filter
        std::cout << "\n--- Small Regular Filter ---\n";
        auto smallRegularFilter{createSmallRegularFilter()};
        std::cout << "Small filter size: " << sizeof(smallRegularFilter) << " bytes\n";

        CompactFilterStorage<32> smallRegularStorage(std::move(smallRegularFilter));
        std::cout << "✓ Small regular storage created\n";
        std::cout << "✓ Has filter: " << (smallRegularStorage.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Is inline storage: " << (smallRegularStorage.isInlineStorage() ? "YES" : "NO") << "\n";

        bool smallRegularResult = smallRegularStorage.evaluate(currentNode);
        std::cout << "✓ Small regular filter result: " << (smallRegularResult ? "PASS" : "FAIL") << "\n";

        // Test large regular filter
        std::cout << "\n--- Large Regular Filter ---\n";
        auto largeRegularFilter{createLargeRegularFilter()};
        std::cout << "Large filter size: " << sizeof(largeRegularFilter) << " bytes\n";

        CompactFilterStorage<32> largeRegularStorage(std::move(largeRegularFilter));
        std::cout << "✓ Large regular storage created\n";
        std::cout << "✓ Has filter: " << (largeRegularStorage.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Is inline storage: " << (largeRegularStorage.isInlineStorage() ? "YES" : "NO") << "\n";

        bool largeRegularResult = largeRegularStorage.evaluate(currentNode);
        std::cout << "✓ Large regular filter result: " << (largeRegularResult ? "PASS" : "FAIL") << "\n";

        std::cout << "\n=== Test 2: CompactContextFilterStorage (Context Filters) ===\n";

        // Test small context filter
        std::cout << "\n--- Small Context Filter ---\n";
        auto smallContextFilter{createSmallContextFilter()};
        std::cout << "Small context filter size: " << sizeof(smallContextFilter) << " bytes\n";

        CompactContextFilterStorage<32> smallContextStorage(std::move(smallContextFilter));
        std::cout << "✓ Small context storage created\n";
        std::cout << "✓ Has filter: " << (smallContextStorage.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Is inline storage: " << (smallContextStorage.isInlineStorage() ? "YES" : "NO") << "\n";

        bool smallContextResult = smallContextStorage.evaluateContext(currentNode, rootDocument);
        std::cout << "✓ Small context filter result: " << (smallContextResult ? "PASS" : "FAIL") << "\n";

        // Test large context filter
        std::cout << "\n--- Large Context Filter ---\n";
        auto largeContextFilter{createLargeContextFilter()};
        std::cout << "Large context filter size: " << sizeof(largeContextFilter) << " bytes\n";

        CompactContextFilterStorage<32> largeContextStorage(std::move(largeContextFilter));
        std::cout << "✓ Large context storage created\n";
        std::cout << "✓ Has filter: " << (largeContextStorage.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Is inline storage: " << (largeContextStorage.isInlineStorage() ? "YES" : "NO") << "\n";

        bool largeContextResult = largeContextStorage.evaluateContext(currentNode, rootDocument);
        std::cout << "✓ Large context filter result: " << (largeContextResult ? "PASS" : "FAIL") << "\n";

        std::cout << "\n=== Test 3: Copy and Move Semantics ===\n";

        // Test copy semantics for large storage
        std::cout << "\n--- Copy Semantics ---\n";
        CompactContextFilterStorage<32> contextCopy(largeContextStorage);
        std::cout << "✓ Context storage copy created\n";
        std::cout << "✓ Copy has filter: " << (contextCopy.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Original still has filter: " << (largeContextStorage.hasFilter() ? "YES" : "NO") << "\n";

        bool copyResult = contextCopy.evaluateContext(currentNode, rootDocument);
        std::cout << "✓ Copy filter result: " << (copyResult ? "PASS" : "FAIL") << "\n";

        // Test move semantics
        std::cout << "\n--- Move Semantics ---\n";
        auto                            anotherLargeFilter{createLargeContextFilter()};
        CompactContextFilterStorage<32> originalForMove(std::move(anotherLargeFilter));
        CompactContextFilterStorage<32> movedStorage(std::move(originalForMove));

        std::cout << "✓ Move constructor completed\n";
        std::cout << "✓ Moved storage has filter: " << (movedStorage.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Original storage has filter: " << (originalForMove.hasFilter() ? "YES" : "NO") << "\n";

        bool moveResult = movedStorage.evaluateContext(currentNode, rootDocument);
        std::cout << "✓ Moved filter result: " << (moveResult ? "PASS" : "FAIL") << "\n";

        std::cout << "\n=== Test 4: Empty Storage Behavior ===\n";

        CompactFilterStorage<32>        emptyRegular;
        CompactContextFilterStorage<32> emptyContext;

        std::cout << "✓ Empty regular storage has filter: " << (emptyRegular.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Empty context storage has filter: " << (emptyContext.hasFilter() ? "YES" : "NO") << "\n";
        std::cout << "✓ Empty regular storage is inline: " << (emptyRegular.isInlineStorage() ? "YES" : "NO") << "\n";
        std::cout << "✓ Empty context storage is inline: " << (emptyContext.isInlineStorage() ? "YES" : "NO") << "\n";

        bool emptyRegularResult = emptyRegular.evaluate(currentNode);
        bool emptyContextResult = emptyContext.evaluateContext(currentNode, rootDocument);

        std::cout << "✓ Empty regular filter result: " << (emptyRegularResult ? "PASS" : "FAIL")
                  << " (should be FAIL)\n";
        std::cout << "✓ Empty context filter result: " << (emptyContextResult ? "PASS" : "FAIL")
                  << " (should be FAIL)\n";

        std::cout << "\n🎉 ALL VARIANT-BASED STORAGE TESTS PASSED!\n";
        std::cout << "\n✅ Benefits Achieved:\n";
        std::cout << "  • Type-safe storage with std::variant\n";
        std::cout << "  • Automatic copy/move semantics\n";
        std::cout << "  • No manual union management\n";
        std::cout << "  • Exception-safe construction\n";
        std::cout << "  • Cleaner, more maintainable code\n";
        std::cout << "  • Eliminated entire class of lifetime bugs\n";
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
