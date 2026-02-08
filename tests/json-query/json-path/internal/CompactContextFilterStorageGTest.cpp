// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// CompactContextFilterStorageGTest.cpp
// ---------------------------------------------------------------------------
// GoogleTest suite for CompactContextFilterStorage comprehensive validation
//
// Tests the CompactContextFilterStorage implementation to ensure proper
// functionality for both small (inline) and large (heap) lambda storage,
// including comprehensive copy/move semantics and evaluation correctness.
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "json-query/json-path/JSONPathCompile.hpp"

#include <QJsonValue>
#include <QJsonObject>
#include <QJsonDocument>
#include <vector>
#include <memory>

using namespace json_query::json_path;

// ---------------------------------------------------------------------------
// Test Fixtures and Helper Functions
// ---------------------------------------------------------------------------

class CompactContextFilterStorageTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create comprehensive test JSON data
        testObj["test"]   = "value";
        testObj["other"]  = 123;
        testObj["nested"] = QJsonObject{{"inner", "data"}};

        rootObj["data"]     = testObj;
        rootObj["metadata"] = QJsonObject{{"version", "1.0"}};

        currentNode  = QJsonValue(testObj);
        rootDocument = QJsonValue(rootObj);

        // Create empty test data
        emptyNode = QJsonValue(QJsonObject{});
    }

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

    // Create a small lambda that fits in inline storage
    auto createSmallLambda()
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

    // Create a lambda that uses root document context
    auto createContextAwareLambda()
    {
        return [](const QJsonValue& currentNode, const QJsonValue& rootDocument) -> bool
        {
            if (currentNode.isObject() && rootDocument.isObject())
            {
                QJsonObject current = currentNode.toObject();
                QJsonObject root    = rootDocument.toObject();
                return current.contains("test") && root.contains("metadata");
            }
            return false;
        };
    }

    QJsonObject testObj;
    QJsonObject rootObj;
    QJsonValue  currentNode;
    QJsonValue  rootDocument;
    QJsonValue  emptyNode;
};

// ---------------------------------------------------------------------------
// Small Lambda (Inline Storage) Tests
// ---------------------------------------------------------------------------

TEST_F(CompactContextFilterStorageTest, SmallLambdaBasicProperties)
{
    auto smallLambda = createSmallLambda();

    EXPECT_LE(sizeof(smallLambda), 32) << "Small lambda should fit in buffer size";

    CompactContextFilterStorage<32> storage(std::move(smallLambda));

    EXPECT_TRUE(storage.hasFilter()) << "Storage should have a filter";
    EXPECT_TRUE(storage.isInlineStorage()) << "Small lambda should use inline storage";
}

TEST_F(CompactContextFilterStorageTest, SmallLambdaEvaluation)
{
    auto                            smallLambda = createSmallLambda();
    CompactContextFilterStorage<32> storage(std::move(smallLambda));

    bool result = storage.evaluateContext(currentNode, rootDocument);
    EXPECT_TRUE(result) << "Small filter should return true for object with 'test' property";

    bool emptyResult = storage.evaluateContext(emptyNode, rootDocument);
    EXPECT_FALSE(emptyResult) << "Small filter should return false for empty object";
}

// ---------------------------------------------------------------------------
// Large Lambda (Heap Storage) Tests
// ---------------------------------------------------------------------------

TEST_F(CompactContextFilterStorageTest, LargeLambdaBasicProperties)
{
    auto largeLambda = createLargeLambda();

    EXPECT_GT(sizeof(largeLambda), 32) << "Large lambda should exceed buffer size";

    CompactContextFilterStorage<32> storage(std::move(largeLambda));

    EXPECT_TRUE(storage.hasFilter()) << "Storage should have a filter";
    EXPECT_FALSE(storage.isInlineStorage()) << "Large lambda should use heap allocation";
}

TEST_F(CompactContextFilterStorageTest, LargeLambdaEvaluation)
{
    auto                            largeLambda = createLargeLambda();
    CompactContextFilterStorage<32> storage(std::move(largeLambda));

    // This is the critical test - where crashes would occur with lifetime issues
    bool result = storage.evaluateContext(currentNode, rootDocument);
    EXPECT_TRUE(result) << "Large filter should return true for object with 'test' property";

    bool emptyResult = storage.evaluateContext(emptyNode, rootDocument);
    EXPECT_FALSE(emptyResult) << "Large filter should return false for empty object";
}

// ---------------------------------------------------------------------------
// Context-Aware Lambda Tests
// ---------------------------------------------------------------------------

TEST_F(CompactContextFilterStorageTest, ContextAwareLambdaEvaluation)
{
    auto                            contextLambda = createContextAwareLambda();
    CompactContextFilterStorage<32> storage(std::move(contextLambda));

    bool result = storage.evaluateContext(currentNode, rootDocument);
    EXPECT_TRUE(result) << "Context-aware filter should return true when both conditions met";

    // Test with different root document
    QJsonObject simpleRoot;
    QJsonValue  simpleRootDoc(simpleRoot);

    bool simpleResult = storage.evaluateContext(currentNode, simpleRootDoc);
    EXPECT_FALSE(simpleResult) << "Context-aware filter should return false without metadata in root";
}

// ---------------------------------------------------------------------------
// Copy Semantics Tests
// ---------------------------------------------------------------------------

TEST_F(CompactContextFilterStorageTest, SmallLambdaCopySemantics)
{
    auto                            smallLambda = createSmallLambda();
    CompactContextFilterStorage<32> original(std::move(smallLambda));

    CompactContextFilterStorage<32> copy(original);

    EXPECT_TRUE(copy.hasFilter()) << "Copy should have a filter";
    EXPECT_TRUE(copy.isInlineStorage()) << "Copy should use inline storage";

    // Test both original and copy work independently
    bool originalResult = original.evaluateContext(currentNode, rootDocument);
    bool copyResult     = copy.evaluateContext(currentNode, rootDocument);

    EXPECT_TRUE(originalResult) << "Original should work after copy";
    EXPECT_TRUE(copyResult) << "Copy should work correctly";
    EXPECT_EQ(originalResult, copyResult) << "Results should be identical";
}

TEST_F(CompactContextFilterStorageTest, LargeLambdaCopySemantics)
{
    auto                            largeLambda = createLargeLambda();
    CompactContextFilterStorage<32> original(std::move(largeLambda));

    // Critical test: copy constructor with heap-allocated lambda
    CompactContextFilterStorage<32> copy(original);

    EXPECT_TRUE(copy.hasFilter()) << "Copy should have a filter";
    EXPECT_FALSE(copy.isInlineStorage()) << "Copy should use heap allocation";

    // Test both original and copy work independently
    bool originalResult = original.evaluateContext(currentNode, rootDocument);
    bool copyResult     = copy.evaluateContext(currentNode, rootDocument);

    EXPECT_TRUE(originalResult) << "Original should work after copy";
    EXPECT_TRUE(copyResult) << "Copy should work correctly";
    EXPECT_EQ(originalResult, copyResult) << "Results should be identical";
}

// ---------------------------------------------------------------------------
// Move Semantics Tests
// ---------------------------------------------------------------------------

TEST_F(CompactContextFilterStorageTest, SmallLambdaMoveSemantics)
{
    auto                            smallLambda = createSmallLambda();
    CompactContextFilterStorage<32> original(std::move(smallLambda));

    CompactContextFilterStorage<32> moved(std::move(original));

    EXPECT_TRUE(moved.hasFilter()) << "Moved storage should have a filter";
    EXPECT_TRUE(moved.isInlineStorage()) << "Moved storage should use inline storage";
    // Note: std::variant move doesn't reset the source to EmptyStorage.
    // The moved-from object is in a valid but unspecified state.

    bool moveResult = moved.evaluateContext(currentNode, rootDocument);
    EXPECT_TRUE(moveResult) << "Moved filter should work correctly";
}

TEST_F(CompactContextFilterStorageTest, LargeLambdaMoveSemantics)
{
    auto                            largeLambda = createLargeLambda();
    CompactContextFilterStorage<32> original(std::move(largeLambda));

    CompactContextFilterStorage<32> moved(std::move(original));

    EXPECT_TRUE(moved.hasFilter()) << "Moved storage should have a filter";
    EXPECT_FALSE(moved.isInlineStorage()) << "Moved storage should use heap allocation";

    bool moveResult = moved.evaluateContext(currentNode, rootDocument);
    EXPECT_TRUE(moveResult) << "Moved filter should work correctly";
}

// ---------------------------------------------------------------------------
// Assignment Operator Tests
// ---------------------------------------------------------------------------

TEST_F(CompactContextFilterStorageTest, CopyAssignmentOperator)
{
    auto                            largeLambda = createLargeLambda();
    CompactContextFilterStorage<32> source(std::move(largeLambda));

    CompactContextFilterStorage<32> target;
    EXPECT_FALSE(target.hasFilter()) << "Target should start empty";

    target = source;

    EXPECT_TRUE(target.hasFilter()) << "Target should have filter after assignment";
    EXPECT_FALSE(target.isInlineStorage()) << "Target should use heap allocation";

    bool sourceResult = source.evaluateContext(currentNode, rootDocument);
    bool targetResult = target.evaluateContext(currentNode, rootDocument);

    EXPECT_TRUE(sourceResult) << "Source should still work";
    EXPECT_TRUE(targetResult) << "Target should work after assignment";
    EXPECT_EQ(sourceResult, targetResult) << "Results should be identical";
}

TEST_F(CompactContextFilterStorageTest, MoveAssignmentOperator)
{
    auto                            largeLambda = createLargeLambda();
    CompactContextFilterStorage<32> source(std::move(largeLambda));

    CompactContextFilterStorage<32> target;
    EXPECT_FALSE(target.hasFilter()) << "Target should start empty";

    target = std::move(source);

    EXPECT_TRUE(target.hasFilter()) << "Target should have filter after move assignment";
    EXPECT_FALSE(target.isInlineStorage()) << "Target should use heap allocation";
    // Note: std::variant move doesn't reset the source to EmptyStorage.

    bool targetResult = target.evaluateContext(currentNode, rootDocument);
    EXPECT_TRUE(targetResult) << "Target should work after move assignment";
}

// ---------------------------------------------------------------------------
// Edge Cases and Stress Tests
// ---------------------------------------------------------------------------

TEST_F(CompactContextFilterStorageTest, MultipleEvaluations)
{
    auto                            largeLambda = createLargeLambda();
    CompactContextFilterStorage<32> storage(std::move(largeLambda));

    // Test multiple evaluations to ensure stability
    for (int i = 0; i < 100; ++i)
    {
        bool result = storage.evaluateContext(currentNode, rootDocument);
        EXPECT_TRUE(result) << "Filter should consistently return true on iteration " << i;
    }
}

TEST_F(CompactContextFilterStorageTest, EmptyStorageOperations)
{
    CompactContextFilterStorage<32> emptyStorage;

    EXPECT_FALSE(emptyStorage.hasFilter()) << "Empty storage should not have a filter";
    EXPECT_FALSE(emptyStorage.isInlineStorage()) << "Empty storage is EmptyStorage, not InlineStorage";

    // Test copy and move of empty storage
    CompactContextFilterStorage<32> emptyCopy(emptyStorage);
    EXPECT_FALSE(emptyCopy.hasFilter()) << "Copy of empty storage should be empty";

    CompactContextFilterStorage<32> emptyMoved(std::move(emptyStorage));
    EXPECT_FALSE(emptyMoved.hasFilter()) << "Move of empty storage should be empty";
}

TEST_F(CompactContextFilterStorageTest, ChainedOperations)
{
    auto                            largeLambda = createLargeLambda();
    CompactContextFilterStorage<32> original(std::move(largeLambda));

    // Test chained copy and move operations
    CompactContextFilterStorage<32> copy1(original);
    CompactContextFilterStorage<32> copy2(copy1);
    CompactContextFilterStorage<32> moved1(std::move(copy2));
    CompactContextFilterStorage<32> moved2(std::move(moved1));

    EXPECT_TRUE(moved2.hasFilter()) << "Final moved storage should have filter";
    EXPECT_FALSE(moved2.isInlineStorage()) << "Final moved storage should use heap allocation";

    bool result = moved2.evaluateContext(currentNode, rootDocument);
    EXPECT_TRUE(result) << "Final moved filter should work correctly";

    // Original and first copy should still work
    bool originalResult = original.evaluateContext(currentNode, rootDocument);
    bool copy1Result    = copy1.evaluateContext(currentNode, rootDocument);

    EXPECT_TRUE(originalResult) << "Original should still work";
    EXPECT_TRUE(copy1Result) << "First copy should still work";
}
