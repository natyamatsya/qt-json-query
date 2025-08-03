// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// EmbeddedFilterStorageGTest.cpp
// ---------------------------------------------------------------------------
// GoogleTest suite for EmbeddedFilter variant-based storage validation
//
// Tests the EmbeddedFilter implementation that combines both regular and
// context filter storage using variant-based approach with small buffer
// optimization for both filter types.
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

class EmbeddedFilterStorageTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        // Create comprehensive test JSON data
        testObj["test"]   = "value";
        testObj["other"]  = 123;
        testObj["nested"] = QJsonObject{{"inner", "data"}};
        testObj["array"]  = QJsonArray{1, 2, 3};

        rootObj["data"]     = testObj;
        rootObj["metadata"] = QJsonObject{{"version", "1.0"}};
        rootObj["config"]   = QJsonObject{{"enabled", true}};

        currentNode  = QJsonValue(testObj);
        rootDocument = QJsonValue(rootObj);

        // Create empty test data
        emptyNode = QJsonValue(QJsonObject{});
    }

    // Create large regular filter (no context)
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

    // Create large context filter
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

    // Create small regular filter
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

    // Create small context filter
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

    // Create context-aware filter that uses root document
    auto createContextAwareFilter()
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
// Regular Filter Storage Tests
// ---------------------------------------------------------------------------

TEST_F(EmbeddedFilterStorageTest, SmallRegularFilterStorage)
{
    auto smallFilter = createSmallRegularFilter();

    EXPECT_LE(sizeof(smallFilter), 32) << "Small regular filter should fit in buffer";

    EmbeddedFilter embeddedFilter;
    embeddedFilter.embedFilter(std::move(smallFilter));

    EXPECT_TRUE(embeddedFilter.hasRegularFilter()) << "Should have regular filter";
    EXPECT_FALSE(embeddedFilter.hasContextFilter()) << "Should not have context filter";

    bool result = embeddedFilter.evaluateEmbeddedFilter(currentNode);
    EXPECT_TRUE(result) << "Small regular filter should work correctly";
}

TEST_F(EmbeddedFilterStorageTest, LargeRegularFilterStorage)
{
    auto largeFilter = createLargeRegularFilter();

    EXPECT_GT(sizeof(largeFilter), 32) << "Large regular filter should exceed buffer size";

    EmbeddedFilter embeddedFilter;
    embeddedFilter.embedFilter(std::move(largeFilter));

    EXPECT_TRUE(embeddedFilter.hasRegularFilter()) << "Should have regular filter";
    EXPECT_FALSE(embeddedFilter.hasContextFilter()) << "Should not have context filter";

    bool result = embeddedFilter.evaluateEmbeddedFilter(currentNode);
    EXPECT_TRUE(result) << "Large regular filter should work correctly";
}

// ---------------------------------------------------------------------------
// Context Filter Storage Tests
// ---------------------------------------------------------------------------

TEST_F(EmbeddedFilterStorageTest, SmallContextFilterStorage)
{
    auto smallContextFilter = createSmallContextFilter();

    EXPECT_LE(sizeof(smallContextFilter), 32) << "Small context filter should fit in buffer";

    EmbeddedFilter embeddedFilter;
    embeddedFilter.embedContextFilter(std::move(smallContextFilter));

    EXPECT_FALSE(embeddedFilter.hasRegularFilter()) << "Should not have regular filter";
    EXPECT_TRUE(embeddedFilter.hasContextFilter()) << "Should have context filter";

    bool result = embeddedFilter.evaluateEmbeddedContextFilter(currentNode, rootDocument);
    EXPECT_TRUE(result) << "Small context filter should work correctly";
}

TEST_F(EmbeddedFilterStorageTest, LargeContextFilterStorage)
{
    auto largeContextFilter = createLargeContextFilter();

    EXPECT_GT(sizeof(largeContextFilter), 32) << "Large context filter should exceed buffer size";

    EmbeddedFilter embeddedFilter;
    embeddedFilter.embedContextFilter(std::move(largeContextFilter));

    EXPECT_FALSE(embeddedFilter.hasRegularFilter()) << "Should not have regular filter";
    EXPECT_TRUE(embeddedFilter.hasContextFilter()) << "Should have context filter";

    bool result = embeddedFilter.evaluateEmbeddedContextFilter(currentNode, rootDocument);
    EXPECT_TRUE(result) << "Large context filter should work correctly";
}

// ---------------------------------------------------------------------------
// Context-Aware Filter Tests
// ---------------------------------------------------------------------------

TEST_F(EmbeddedFilterStorageTest, ContextAwareFilterEvaluation)
{
    auto contextAwareFilter = createContextAwareFilter();

    EmbeddedFilter embeddedFilter;
    embeddedFilter.embedContextFilter(std::move(contextAwareFilter));

    bool result = embeddedFilter.evaluateEmbeddedContextFilter(currentNode, rootDocument);
    EXPECT_TRUE(result) << "Context-aware filter should return true when both conditions met";

    // Test with different root document
    QJsonObject simpleRoot;
    QJsonValue  simpleRootDoc(simpleRoot);

    bool simpleResult = embeddedFilter.evaluateEmbeddedContextFilter(currentNode, simpleRootDoc);
    EXPECT_FALSE(simpleResult) << "Context-aware filter should return false without metadata in root";
}

// ---------------------------------------------------------------------------
// Copy Semantics Tests
// ---------------------------------------------------------------------------

TEST_F(EmbeddedFilterStorageTest, RegularFilterCopySemantics)
{
    auto regularFilter = createLargeRegularFilter();

    EmbeddedFilter original;
    original.embedFilter(std::move(regularFilter));

    EmbeddedFilter copy(original);

    EXPECT_TRUE(copy.hasRegularFilter()) << "Copy should have regular filter";
    EXPECT_FALSE(copy.hasContextFilter()) << "Copy should not have context filter";

    bool originalResult = original.evaluateEmbeddedFilter(currentNode);
    bool copyResult     = copy.evaluateEmbeddedFilter(currentNode);

    EXPECT_TRUE(originalResult) << "Original should work after copy";
    EXPECT_TRUE(copyResult) << "Copy should work correctly";
    EXPECT_EQ(originalResult, copyResult) << "Results should be identical";
}

TEST_F(EmbeddedFilterStorageTest, ContextFilterCopySemantics)
{
    auto contextFilter = createLargeContextFilter();

    EmbeddedFilter original;
    original.embedContextFilter(std::move(contextFilter));

    EmbeddedFilter copy(original);

    EXPECT_FALSE(copy.hasRegularFilter()) << "Copy should not have regular filter";
    EXPECT_TRUE(copy.hasContextFilter()) << "Copy should have context filter";

    bool originalResult = original.evaluateEmbeddedContextFilter(currentNode, rootDocument);
    bool copyResult     = copy.evaluateEmbeddedContextFilter(currentNode, rootDocument);

    EXPECT_TRUE(originalResult) << "Original should work after copy";
    EXPECT_TRUE(copyResult) << "Copy should work correctly";
    EXPECT_EQ(originalResult, copyResult) << "Results should be identical";
}

// ---------------------------------------------------------------------------
// Move Semantics Tests
// ---------------------------------------------------------------------------

TEST_F(EmbeddedFilterStorageTest, RegularFilterMoveSemantics)
{
    auto regularFilter = createLargeRegularFilter();

    EmbeddedFilter original;
    original.embedFilter(std::move(regularFilter));

    EmbeddedFilter moved(std::move(original));

    EXPECT_TRUE(moved.hasRegularFilter()) << "Moved should have regular filter";
    EXPECT_FALSE(moved.hasContextFilter()) << "Moved should not have context filter";
    EXPECT_FALSE(original.hasRegularFilter()) << "Original should be empty after move";
    EXPECT_FALSE(original.hasContextFilter()) << "Original should be empty after move";

    bool result = moved.evaluateEmbeddedFilter(currentNode);
    EXPECT_TRUE(result) << "Moved filter should work correctly";
}

TEST_F(EmbeddedFilterStorageTest, ContextFilterMoveSemantics)
{
    auto contextFilter = createLargeContextFilter();

    EmbeddedFilter original;
    original.embedContextFilter(std::move(contextFilter));

    EmbeddedFilter moved(std::move(original));

    EXPECT_FALSE(moved.hasRegularFilter()) << "Moved should not have regular filter";
    EXPECT_TRUE(moved.hasContextFilter()) << "Moved should have context filter";
    EXPECT_FALSE(original.hasRegularFilter()) << "Original should be empty after move";
    EXPECT_FALSE(original.hasContextFilter()) << "Original should be empty after move";

    bool result = moved.evaluateEmbeddedContextFilter(currentNode, rootDocument);
    EXPECT_TRUE(result) << "Moved context filter should work correctly";
}

// ---------------------------------------------------------------------------
// Assignment Operator Tests
// ---------------------------------------------------------------------------

TEST_F(EmbeddedFilterStorageTest, RegularFilterAssignment)
{
    auto regularFilter = createLargeRegularFilter();

    EmbeddedFilter source;
    source.embedFilter(std::move(regularFilter));

    EmbeddedFilter target;
    EXPECT_FALSE(target.hasRegularFilter()) << "Target should start empty";
    EXPECT_FALSE(target.hasContextFilter()) << "Target should start empty";

    target = source;

    EXPECT_TRUE(target.hasRegularFilter()) << "Target should have regular filter after assignment";
    EXPECT_FALSE(target.hasContextFilter()) << "Target should not have context filter";

    bool sourceResult = source.evaluateEmbeddedFilter(currentNode);
    bool targetResult = target.evaluateEmbeddedFilter(currentNode);

    EXPECT_TRUE(sourceResult) << "Source should still work";
    EXPECT_TRUE(targetResult) << "Target should work after assignment";
    EXPECT_EQ(sourceResult, targetResult) << "Results should be identical";
}

TEST_F(EmbeddedFilterStorageTest, ContextFilterAssignment)
{
    auto contextFilter = createLargeContextFilter();

    EmbeddedFilter source;
    source.embedContextFilter(std::move(contextFilter));

    EmbeddedFilter target;
    target = source;

    EXPECT_FALSE(target.hasRegularFilter()) << "Target should not have regular filter";
    EXPECT_TRUE(target.hasContextFilter()) << "Target should have context filter after assignment";

    bool sourceResult = source.evaluateEmbeddedContextFilter(currentNode, rootDocument);
    bool targetResult = target.evaluateEmbeddedContextFilter(currentNode, rootDocument);

    EXPECT_TRUE(sourceResult) << "Source should still work";
    EXPECT_TRUE(targetResult) << "Target should work after assignment";
    EXPECT_EQ(sourceResult, targetResult) << "Results should be identical";
}

// ---------------------------------------------------------------------------
// Mixed Filter Type Tests
// ---------------------------------------------------------------------------

TEST_F(EmbeddedFilterStorageTest, FilterTypeTransition)
{
    auto regularFilter = createSmallRegularFilter();

    EmbeddedFilter embeddedFilter;
    embeddedFilter.embedFilter(std::move(regularFilter));

    EXPECT_TRUE(embeddedFilter.hasRegularFilter()) << "Should initially have regular filter";
    EXPECT_FALSE(embeddedFilter.hasContextFilter()) << "Should not have context filter";

    // Transition to context filter
    auto contextFilter = createSmallContextFilter();
    embeddedFilter.embedContextFilter(std::move(contextFilter));

    EXPECT_FALSE(embeddedFilter.hasRegularFilter()) << "Should no longer have regular filter";
    EXPECT_TRUE(embeddedFilter.hasContextFilter()) << "Should now have context filter";

    bool result = embeddedFilter.evaluateEmbeddedContextFilter(currentNode, rootDocument);
    EXPECT_TRUE(result) << "Context filter should work after transition";
}

// ---------------------------------------------------------------------------
// Edge Cases and Error Handling Tests
// ---------------------------------------------------------------------------

TEST_F(EmbeddedFilterStorageTest, EmptyFilterOperations)
{
    EmbeddedFilter emptyFilter;

    EXPECT_FALSE(emptyFilter.hasRegularFilter()) << "Empty filter should not have regular filter";
    EXPECT_FALSE(emptyFilter.hasContextFilter()) << "Empty filter should not have context filter";

    // Test copy and move of empty filter
    EmbeddedFilter emptyCopy(emptyFilter);
    EXPECT_FALSE(emptyCopy.hasRegularFilter()) << "Copy of empty filter should be empty";
    EXPECT_FALSE(emptyCopy.hasContextFilter()) << "Copy of empty filter should be empty";

    EmbeddedFilter emptyMoved(std::move(emptyFilter));
    EXPECT_FALSE(emptyMoved.hasRegularFilter()) << "Move of empty filter should be empty";
    EXPECT_FALSE(emptyMoved.hasContextFilter()) << "Move of empty filter should be empty";
}

TEST_F(EmbeddedFilterStorageTest, MultipleEvaluations)
{
    auto largeContextFilter = createLargeContextFilter();

    EmbeddedFilter embeddedFilter;
    embeddedFilter.embedContextFilter(std::move(largeContextFilter));

    // Test multiple evaluations to ensure stability
    for (int i = 0; i < 50; ++i)
    {
        bool result = embeddedFilter.evaluateEmbeddedContextFilter(currentNode, rootDocument);
        EXPECT_TRUE(result) << "Filter should consistently return true on iteration " << i;
    }
}

TEST_F(EmbeddedFilterStorageTest, EvaluationWithDifferentData)
{
    auto contextFilter = createContextAwareFilter();

    EmbeddedFilter embeddedFilter;
    embeddedFilter.embedContextFilter(std::move(contextFilter));

    // Test with empty node
    bool emptyResult = embeddedFilter.evaluateEmbeddedContextFilter(emptyNode, rootDocument);
    EXPECT_FALSE(emptyResult) << "Filter should return false for empty object";

    // Test with node that has test property but different root
    QJsonObject differentRoot;
    differentRoot["other"] = "data";
    QJsonValue differentRootDoc(differentRoot);

    bool differentRootResult = embeddedFilter.evaluateEmbeddedContextFilter(currentNode, differentRootDoc);
    EXPECT_FALSE(differentRootResult) << "Filter should return false when root lacks metadata";
}

// ---------------------------------------------------------------------------
// Performance and Stress Tests
// ---------------------------------------------------------------------------

TEST_F(EmbeddedFilterStorageTest, ChainedOperations)
{
    auto largeRegularFilter = createLargeRegularFilter();

    EmbeddedFilter original;
    original.embedFilter(std::move(largeRegularFilter));

    // Test chained copy and move operations
    EmbeddedFilter copy1(original);
    EmbeddedFilter copy2(copy1);
    EmbeddedFilter moved1(std::move(copy2));
    EmbeddedFilter moved2(std::move(moved1));

    EXPECT_TRUE(moved2.hasRegularFilter()) << "Final moved filter should have regular filter";
    EXPECT_FALSE(moved2.hasContextFilter()) << "Final moved filter should not have context filter";

    bool result = moved2.evaluateEmbeddedFilter(currentNode);
    EXPECT_TRUE(result) << "Final moved filter should work correctly";

    // Original and first copy should still work
    bool originalResult = original.evaluateEmbeddedFilter(currentNode);
    bool copy1Result    = copy1.evaluateEmbeddedFilter(currentNode);

    EXPECT_TRUE(originalResult) << "Original should still work";
    EXPECT_TRUE(copy1Result) << "First copy should still work";
}

TEST_F(EmbeddedFilterStorageTest, MixedFilterTypeChaining)
{
    // Start with regular filter
    auto           regularFilter = createSmallRegularFilter();
    EmbeddedFilter filter1;
    filter1.embedFilter(std::move(regularFilter));

    // Copy to filter2 and transition to context filter
    EmbeddedFilter filter2(filter1);
    auto           contextFilter = createSmallContextFilter();
    filter2.embedContextFilter(std::move(contextFilter));

    // Move filter2 to filter3
    EmbeddedFilter filter3(std::move(filter2));

    // Verify states
    EXPECT_TRUE(filter1.hasRegularFilter()) << "Filter1 should still have regular filter";
    EXPECT_FALSE(filter1.hasContextFilter()) << "Filter1 should not have context filter";

    EXPECT_FALSE(filter2.hasRegularFilter()) << "Filter2 should be empty after move";
    EXPECT_FALSE(filter2.hasContextFilter()) << "Filter2 should be empty after move";

    EXPECT_FALSE(filter3.hasRegularFilter()) << "Filter3 should not have regular filter";
    EXPECT_TRUE(filter3.hasContextFilter()) << "Filter3 should have context filter";

    // Test evaluations
    bool result1 = filter1.evaluateEmbeddedFilter(currentNode);
    bool result3 = filter3.evaluateEmbeddedContextFilter(currentNode, rootDocument);

    EXPECT_TRUE(result1) << "Filter1 regular filter should work";
    EXPECT_TRUE(result3) << "Filter3 context filter should work";
}
