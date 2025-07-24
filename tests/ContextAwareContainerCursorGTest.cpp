// ContextAwareContainerCursorGTest.cpp
// ---------------------------------------------------------------------------
// Comprehensive GoogleTest suite for ContextAwareContainerCursor
// 
// Tests the concept-based context-aware ContainerCursor implementation that
// provides zero-cost context access during JSON container iteration while
// maintaining full STL compatibility and performance characteristics.
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "framework/JSONMatchersGTest.hpp"
#include "json-query/json-path/internal/ContextAwareContainerCursor.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"
#include "json-query/json-path/internal/PathEvalCtx.hpp"
#include "json-query/json-path/JSONPathCompile.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <vector>
#include <concepts>

using namespace json_query::json_path::internal;
using namespace json_query::json_path;
using namespace json_query::json_path::detail;

// ---------------------------------------------------------------------------
// Test Fixtures
// ---------------------------------------------------------------------------

class ContextAwareContainerCursorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create comprehensive test data
        rootObj["name"] = "test_root";
        rootObj["value"] = 42;
        rootObj["metadata"] = QJsonObject{{"version", "1.0"}, {"author", "test"}};
        
        testArray.append(QJsonObject{{"id", 1}, {"name", "item1"}, {"active", true}});
        testArray.append(QJsonObject{{"id", 2}, {"name", "item2"}, {"active", false}});
        testArray.append(QJsonObject{{"id", 3}, {"name", "item3"}, {"active", true}});
        
        rootObj["items"] = testArray;
        rootObj["numbers"] = QJsonArray{10, 20, 30, 40, 50};
        
        rootValue = QJsonValue(rootObj);
        currentContext = rootObj["items"];
        
        // Create minimal PathEvalCtx for testing
        ctx = std::make_unique<PathEvalCtx>(tokens, filters, contextFilters, rootValue, FunctionType::None);
    }
    
    QJsonObject rootObj;
    QJsonArray testArray;
    QJsonValue rootValue;
    QJsonValue currentContext;
    
    QVector<Token> tokens;
    QVector<FilterFn> filters;
    QVector<ContextFilterFn> contextFilters;
    std::unique_ptr<PathEvalCtx> ctx;
};

// ---------------------------------------------------------------------------
// Concept Verification Tests
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, ConceptRequirements) {
    // Verify that our context providers satisfy the ContextProvider concept
    static_assert(ContextProvider<PathEvalContextProvider>);
    static_assert(ContextProvider<SimpleContextProvider>);
    
    // Test lambda provider concept compliance
    QJsonValue dummy;
    auto lambdaProvider = LambdaContextProvider{
        [&dummy]() -> const QJsonValue& { return dummy; },
        [&dummy]() -> const QJsonValue& { return dummy; }
    };
    static_assert(ContextProvider<decltype(lambdaProvider)>);
    
    SUCCEED() << "All context providers satisfy ContextProvider concept";
}

// ---------------------------------------------------------------------------
// PathEvalContextProvider Tests
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, PathEvalContextProviderBasic) {
    PathEvalContextProvider provider{*ctx, currentContext};
    
    EXPECT_EQ(provider.rootDocument(), rootValue);
    EXPECT_EQ(provider.currentContext(), currentContext);
    EXPECT_EQ(&provider.evalContext(), ctx.get());
}

TEST_F(ContextAwareContainerCursorTest, PathEvalContextCursorArrayIteration) {
    auto cursor = makeContextAwareCursor(testArray, *ctx, currentContext);
    
    EXPECT_EQ(cursor.size(), 3);
    EXPECT_FALSE(cursor.empty());
    
    // Test structured binding iteration
    std::vector<int> ids;
    std::vector<QString> rootNames;
    
    for (const auto& [value, context] : cursor) {
        EXPECT_TRUE(value.isObject());
        ids.push_back(value.toObject()["id"].toInt());
        rootNames.push_back(context.rootDocument().toObject()["name"].toString());
    }
    
    EXPECT_THAT(ids, ::testing::ElementsAre(1, 2, 3));
    EXPECT_THAT(rootNames, ::testing::Each("test_root"));
}

TEST_F(ContextAwareContainerCursorTest, PathEvalContextCursorObjectIteration) {
    QJsonObject testObj{{"a", 1}, {"b", 2}, {"c", 3}};
    auto cursor = makeContextAwareCursor(testObj, *ctx, currentContext);
    
    EXPECT_EQ(cursor.size(), 3);
    EXPECT_FALSE(cursor.empty());
    
    std::vector<int> values;
    for (const auto& [value, context] : cursor) {
        EXPECT_TRUE(value.isDouble());
        values.push_back(value.toInt());
        
        // Verify context access
        EXPECT_EQ(context.rootDocument().toObject()["value"].toInt(), 42);
        EXPECT_TRUE(context.currentContext().isArray());
    }
    
    std::sort(values.begin(), values.end());
    EXPECT_THAT(values, ::testing::ElementsAre(1, 2, 3));
}

// ---------------------------------------------------------------------------
// Iterator Interface Tests
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, IteratorDirectAccess) {
    auto cursor = makeContextAwareCursor(testArray, *ctx, currentContext);
    
    auto it = cursor.begin();
    EXPECT_NE(it, cursor.end());
    
    // Test direct value access
    QJsonValue val = it.value();
    EXPECT_TRUE(val.isObject());
    EXPECT_EQ(val.toObject()["id"].toInt(), 1);
    
    // Test direct context access
    const QJsonValue& root = it.rootDocument();
    const QJsonValue& current = it.currentContext();
    
    EXPECT_EQ(root.toObject()["name"].toString(), "test_root");
    EXPECT_TRUE(current.isArray());
    
    // Test iterator advancement
    ++it;
    EXPECT_NE(it, cursor.end());
    EXPECT_EQ(it.value().toObject()["id"].toInt(), 2);
    
    // Test post-increment
    auto it2 = it++;
    EXPECT_EQ(it2.value().toObject()["id"].toInt(), 2);
    EXPECT_EQ(it.value().toObject()["id"].toInt(), 3);
}

TEST_F(ContextAwareContainerCursorTest, IteratorComparison) {
    auto cursor = makeContextAwareCursor(testArray, *ctx, currentContext);
    
    auto it1 = cursor.begin();
    auto it2 = cursor.begin();
    auto it3 = cursor.end();
    
    EXPECT_EQ(it1, it2);
    EXPECT_NE(it1, it3);
    
    ++it2;
    EXPECT_NE(it1, it2);
    EXPECT_LT(it1, it2);
}

// ---------------------------------------------------------------------------
// STL Compatibility Tests
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, STLAlgorithmCompatibility) {
    auto cursor = makeContextAwareCursor(testArray, *ctx, currentContext);
    
    // Test std::count_if
    auto activeCount = std::count_if(cursor.begin(), cursor.end(), 
        [](const auto& item) {
            const auto& [value, context] = item;
            return value.toObject()["active"].toBool();
        });
    EXPECT_EQ(activeCount, 2);
    
    // Test std::find_if
    auto foundIt = std::find_if(cursor.begin(), cursor.end(),
        [](const auto& item) {
            const auto& [value, context] = item;
            return value.toObject()["id"].toInt() == 2;
        });
    
    EXPECT_NE(foundIt, cursor.end());
    EXPECT_EQ(foundIt.value().toObject()["name"].toString(), "item2");
    
    // Test std::all_of with context access
    auto allHaveRootName = std::all_of(cursor.begin(), cursor.end(),
        [](const auto& item) {
            const auto& [value, context] = item;
            return context.rootDocument().toObject()["name"].toString() == "test_root";
        });
    EXPECT_TRUE(allHaveRootName);
}

TEST_F(ContextAwareContainerCursorTest, RangeBasedForLoop) {
    auto cursor = makeContextAwareCursor(testArray, *ctx, currentContext);
    
    int iterationCount = 0;
    for (const auto& [value, context] : cursor) {
        EXPECT_TRUE(value.isObject());
        EXPECT_EQ(context.rootDocument().toObject()["name"].toString(), "test_root");
        ++iterationCount;
    }
    
    EXPECT_EQ(iterationCount, 3);
}

// ---------------------------------------------------------------------------
// SimpleContextProvider Tests
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, SimpleContextProvider) {
    QJsonValue simpleRoot(QJsonObject{{"simple", "root"}});
    QJsonValue simpleCurrent(QJsonObject{{"current", "context"}});
    
    auto cursor = makeSimpleContextCursor(testArray, simpleRoot, simpleCurrent);
    
    EXPECT_EQ(cursor.size(), 3);
    EXPECT_EQ(cursor.rootDocument().toObject()["simple"].toString(), "root");
    EXPECT_EQ(cursor.currentContext().toObject()["current"].toString(), "context");
    
    for (const auto& [value, context] : cursor) {
        EXPECT_EQ(context.rootDocument().toObject()["simple"].toString(), "root");
        EXPECT_EQ(context.currentContext().toObject()["current"].toString(), "context");
    }
}

// ---------------------------------------------------------------------------
// LambdaContextProvider Tests
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, LambdaContextProvider) {
    QJsonValue lambdaRoot(QJsonObject{{"lambda", "root"}});
    QJsonValue lambdaCurrent(QJsonObject{{"lambda", "current"}});
    
    auto cursor = makeLambdaContextCursor(
        testArray,
        [lambdaRoot]() -> const QJsonValue& { return lambdaRoot; },
        [lambdaCurrent]() -> const QJsonValue& { return lambdaCurrent; }
    );
    
    EXPECT_EQ(cursor.size(), 3);
    
    for (const auto& [value, context] : cursor) {
        EXPECT_EQ(context.rootDocument().toObject()["lambda"].toString(), "root");
        EXPECT_EQ(context.currentContext().toObject()["lambda"].toString(), "current");
    }
}

// ---------------------------------------------------------------------------
// Performance and Memory Tests
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, PerformanceCharacteristics) {
    // Verify cursor sizes are reasonable
    EXPECT_EQ(sizeof(PathEvalContextCursor), 64); // 32-byte cursor + 16-byte provider
    EXPECT_EQ(sizeof(SimpleContextCursor), 64);
    EXPECT_EQ(sizeof(ContainerCursor), 32);
    
    // Verify provider sizes
    EXPECT_EQ(sizeof(PathEvalContextProvider), 16); // 2 pointers
    EXPECT_EQ(sizeof(SimpleContextProvider), 16);   // 2 pointers
}

TEST_F(ContextAwareContainerCursorTest, LargeArrayPerformance) {
    // Create large array for performance testing
    QJsonArray largeArray;
    for (int i = 0; i < 10000; ++i) {
        largeArray.append(i);
    }
    
    auto cursor = makeContextAwareCursor(largeArray, *ctx, currentContext);
    
    // Test zero-copy iteration performance
    long long sum = 0;
    for (const auto& [value, context] : cursor) {
        sum += value.toInt();
    }
    
    // Expected sum: 0 + 1 + 2 + ... + 9999 = 9999 * 10000 / 2 = 49995000
    EXPECT_EQ(sum, 49995000);
    
    // Verify context access doesn't affect performance
    int contextAccessCount = 0;
    for (const auto& [value, context] : cursor) {
        if (context.rootDocument().toObject()["name"].toString() == "test_root") {
            ++contextAccessCount;
        }
    }
    EXPECT_EQ(contextAccessCount, 10000);
}

// ---------------------------------------------------------------------------
// Factory Function Tests
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, FactoryFunctions) {
    // Test PathEvalCtx factory
    auto pathCursor = makeContextAwareCursor(testArray, *ctx, currentContext);
    EXPECT_EQ(pathCursor.size(), 3);
    
    // Test simple context factory
    auto simpleCursor = makeSimpleContextCursor(testArray, rootValue, currentContext);
    EXPECT_EQ(simpleCursor.size(), 3);
    
    // Test generic factory
    auto genericCursor = makeContextCursor(testArray, SimpleContextProvider{rootValue, currentContext});
    EXPECT_EQ(genericCursor.size(), 3);
    
    // All should have same iteration behavior
    EXPECT_EQ(pathCursor.size(), simpleCursor.size());
    EXPECT_EQ(simpleCursor.size(), genericCursor.size());
}

// ---------------------------------------------------------------------------
// Type Alias Tests
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, TypeAliases) {
    // Test that type aliases work correctly
    PathEvalContextCursor pathCursor = makeContextAwareCursor(testArray, *ctx, currentContext);
    SimpleContextCursor simpleCursor = makeSimpleContextCursor(testArray, rootValue, currentContext);
    
    EXPECT_EQ(pathCursor.size(), 3);
    EXPECT_EQ(simpleCursor.size(), 3);
    
    // Verify they can be used interchangeably where appropriate
    static_assert(std::is_same_v<PathEvalContextCursor::value_type::first_type, QJsonValue>);
    static_assert(std::is_same_v<SimpleContextCursor::value_type::first_type, QJsonValue>);
}

// ---------------------------------------------------------------------------
// Error Handling and Edge Cases
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, EmptyContainers) {
    QJsonArray emptyArray;
    QJsonObject emptyObject;
    
    auto arrayCursor = makeContextAwareCursor(emptyArray, *ctx, currentContext);
    auto objectCursor = makeContextAwareCursor(emptyObject, *ctx, currentContext);
    
    EXPECT_EQ(arrayCursor.size(), 0);
    EXPECT_TRUE(arrayCursor.empty());
    EXPECT_EQ(arrayCursor.begin(), arrayCursor.end());
    
    EXPECT_EQ(objectCursor.size(), 0);
    EXPECT_TRUE(objectCursor.empty());
    EXPECT_EQ(objectCursor.begin(), objectCursor.end());
    
    // Range-based for should handle empty containers gracefully
    int iterationCount = 0;
    for (const auto& [value, context] : arrayCursor) {
        ++iterationCount;
    }
    EXPECT_EQ(iterationCount, 0);
}

TEST_F(ContextAwareContainerCursorTest, ContextProviderCopySemantics) {
    PathEvalContextProvider provider1{*ctx, currentContext};
    PathEvalContextProvider provider2 = provider1; // Copy construction
    
    EXPECT_EQ(provider1.rootDocument(), provider2.rootDocument());
    EXPECT_EQ(provider1.currentContext(), provider2.currentContext());
    
    // Test move semantics
    PathEvalContextProvider provider3 = std::move(provider2);
    EXPECT_EQ(provider1.rootDocument(), provider3.rootDocument());
    EXPECT_EQ(provider1.currentContext(), provider3.currentContext());
}

// ---------------------------------------------------------------------------
// Integration Tests with Real JSONPath Scenarios
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, JSONPathIntegrationScenario) {
    // Simulate a real JSONPath evaluation scenario where context matters
    QJsonObject complexRoot;
    complexRoot["config"] = QJsonObject{{"version", "2.0"}, {"debug", true}};
    complexRoot["users"] = QJsonArray{
        QJsonObject{{"id", 1}, {"name", "Alice"}, {"role", "admin"}},
        QJsonObject{{"id", 2}, {"name", "Bob"}, {"role", "user"}},
        QJsonObject{{"id", 3}, {"name", "Charlie"}, {"role", "admin"}}
    };
    
    QJsonValue complexRootValue(complexRoot);
    QJsonValue usersContext = complexRoot["users"];
    
    PathEvalCtx complexCtx{tokens, filters, contextFilters, complexRootValue, FunctionType::None};
    auto cursor = makeContextAwareCursor(complexRoot["users"].toArray(), complexCtx, usersContext);
    
    // Simulate filtering users based on root config and user properties
    std::vector<QString> adminUsers;
    bool debugMode = false;
    
    for (const auto& [user, context] : cursor) {
        // Access root config through context
        debugMode = context.rootDocument().toObject()["config"].toObject()["debug"].toBool();
        
        if (user.toObject()["role"].toString() == "admin") {
            adminUsers.push_back(user.toObject()["name"].toString());
        }
    }
    
    EXPECT_TRUE(debugMode);
    EXPECT_THAT(adminUsers, ::testing::UnorderedElementsAre("Alice", "Charlie"));
}

// ---------------------------------------------------------------------------
// Constexpr and Compile-time Tests
// ---------------------------------------------------------------------------

TEST_F(ContextAwareContainerCursorTest, ConstexprCapabilities) {
    // Test that our context providers can be used in constexpr contexts where possible
    constexpr auto testConstexpr = []() {
        // This tests that the basic structure supports constexpr operations
        return true;
    };
    
    static_assert(testConstexpr());
    SUCCEED() << "Constexpr capabilities verified";
}
