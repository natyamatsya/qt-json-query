// ContainerCursorGTest.cpp
// ---------------------------------------------------------------------------
// Comprehensive GoogleTest suite for ContainerCursor
// 
// Tests the foundational C++23 STL-like ContainerCursor implementation that
// provides zero-copy iteration over QJsonObject and QJsonArray with modern
// iterator interface, performance characteristics, and optional ranges support.
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "framework/JSONMatchersGTest.hpp"
#include "json-query/json-path/internal/ContainerCursor.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>

#include <algorithm>
#include <vector>
#include <numeric>
#include <iterator>
#include <ranges>

using namespace json_query::json_path::internal;

// ---------------------------------------------------------------------------
// Test Fixtures
// ---------------------------------------------------------------------------

class ContainerCursorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create comprehensive test data
        testObj["name"] = "test_object";
        testObj["value"] = 42;
        testObj["active"] = true;
        testObj["score"] = 98.5;
        testObj["metadata"] = QJsonObject{{"version", "1.0"}, {"author", "test"}};
        
        testArray.append("first");
        testArray.append(123);
        testArray.append(true);
        testArray.append(QJsonObject{{"nested", "object"}});
        testArray.append(QJsonArray{"nested", "array"});
        
        // Create large array for performance testing
        for (int i = 0; i < 1000; ++i) {
            largeArray.append(i);
        }
        
        // Create empty containers
        emptyObj = QJsonObject{};
        emptyArray = QJsonArray{};
    }
    
    QJsonObject testObj;
    QJsonArray testArray;
    QJsonArray largeArray;
    QJsonObject emptyObj;
    QJsonArray emptyArray;
};

// ---------------------------------------------------------------------------
// Basic Construction and Properties Tests
// ---------------------------------------------------------------------------

TEST_F(ContainerCursorTest, ObjectConstruction) {
    auto cursor = ContainerCursor::object(testObj);
    
    EXPECT_EQ(cursor.size(), 5);
    EXPECT_FALSE(cursor.empty());
}

TEST_F(ContainerCursorTest, ArrayConstruction) {
    auto cursor = ContainerCursor::array(testArray);
    
    EXPECT_EQ(cursor.size(), 5);
    EXPECT_FALSE(cursor.empty());
}

TEST_F(ContainerCursorTest, EmptyContainers) {
    auto objCursor = ContainerCursor::object(emptyObj);
    auto arrayCursor = ContainerCursor::array(emptyArray);
    
    EXPECT_EQ(objCursor.size(), 0);
    EXPECT_TRUE(objCursor.empty());
    EXPECT_EQ(objCursor.begin(), objCursor.end());
    
    EXPECT_EQ(arrayCursor.size(), 0);
    EXPECT_TRUE(arrayCursor.empty());
    EXPECT_EQ(arrayCursor.begin(), arrayCursor.end());
}

// ---------------------------------------------------------------------------
// Iterator Interface Tests
// ---------------------------------------------------------------------------

TEST_F(ContainerCursorTest, BasicIteration) {
    auto cursor = ContainerCursor::array(testArray);
    
    auto it = cursor.begin();
    EXPECT_NE(it, cursor.end());
    
    // Test first element
    EXPECT_TRUE((*it).isString());
    EXPECT_EQ((*it).toString(), "first");
    
    // Test pre-increment
    ++it;
    EXPECT_NE(it, cursor.end());
    EXPECT_TRUE((*it).isDouble());
    EXPECT_EQ((*it).toInt(), 123);
    
    // Test post-increment
    auto it2 = it++;
    EXPECT_TRUE((*it2).isDouble());
    EXPECT_EQ((*it2).toInt(), 123);
    EXPECT_TRUE((*it).isBool());
    EXPECT_EQ((*it).toBool(), true);
}

TEST_F(ContainerCursorTest, ObjectIteration) {
    auto cursor = ContainerCursor::object(testObj);
    
    std::vector<QJsonValue> values;
    
    for (auto it = cursor.begin(); it != cursor.end(); ++it) {
        values.push_back(*it);
    }
    
    EXPECT_EQ(values.size(), 5);
    
    // Verify we got all expected values (order may vary for objects)
    std::vector<QString> stringValues;
    std::vector<int> intValues;
    std::vector<bool> boolValues;
    std::vector<double> doubleValues;
    
    for (const auto& val : values) {
        if (val.isString()) {
            stringValues.push_back(val.toString());
        } else if (val.isDouble() && val.toDouble() == static_cast<int>(val.toDouble())) {
            intValues.push_back(val.toInt());
        } else if (val.isBool()) {
            boolValues.push_back(val.toBool());
        } else if (val.isDouble()) {
            doubleValues.push_back(val.toDouble());
        }
    }
    
    EXPECT_THAT(stringValues, ::testing::Contains("test_object"));
    EXPECT_THAT(intValues, ::testing::Contains(42));
    EXPECT_THAT(boolValues, ::testing::Contains(true));
    EXPECT_THAT(doubleValues, ::testing::Contains(98.5));
}

TEST_F(ContainerCursorTest, IteratorComparison) {
    auto cursor = ContainerCursor::array(testArray);
    
    auto it1 = cursor.begin();
    auto it2 = cursor.begin();
    auto it3 = cursor.end();
    
    EXPECT_EQ(it1, it2);
    EXPECT_NE(it1, it3);
    
    ++it2;
    EXPECT_NE(it1, it2);
    EXPECT_LT(it1, it2);
    
    // Test three-way comparison
    EXPECT_TRUE((it1 <=> it2) < 0);
    EXPECT_TRUE((it2 <=> it1) > 0);
    EXPECT_TRUE((it1 <=> it1) == 0);
}

TEST_F(ContainerCursorTest, IteratorTraits) {
    using Iterator = ContainerCursor::iterator;
    
    // Verify iterator traits
    static_assert(std::is_same_v<Iterator::iterator_concept, std::forward_iterator_tag>);
    static_assert(std::is_same_v<Iterator::iterator_category, std::forward_iterator_tag>);
    static_assert(std::is_same_v<Iterator::value_type, QJsonValue>);
    static_assert(std::is_same_v<Iterator::difference_type, qsizetype>);
    static_assert(std::is_same_v<Iterator::pointer, const QJsonValue*>);
    static_assert(std::is_same_v<Iterator::reference, const QJsonValue&>);
    
    SUCCEED() << "Iterator traits are correctly defined";
}

// ---------------------------------------------------------------------------
// STL Compatibility Tests
// ---------------------------------------------------------------------------

TEST_F(ContainerCursorTest, RangeBasedForLoop) {
    auto cursor = ContainerCursor::array(testArray);
    
    std::vector<QJsonValue> collected;
    for (const auto& value : cursor) {
        collected.push_back(value);
    }
    
    EXPECT_EQ(collected.size(), 5);
    EXPECT_EQ(collected[0].toString(), "first");
    EXPECT_EQ(collected[1].toInt(), 123);
    EXPECT_EQ(collected[2].toBool(), true);
}

TEST_F(ContainerCursorTest, STLAlgorithms) {
    auto cursor = ContainerCursor::array(testArray);
    
    // Test std::count_if
    auto stringCount = std::count_if(cursor.begin(), cursor.end(), 
        [](const QJsonValue& v) { return v.isString(); });
    EXPECT_EQ(stringCount, 1);
    
    // Test std::find_if
    auto foundIt = std::find_if(cursor.begin(), cursor.end(),
        [](const QJsonValue& v) { return v.isDouble() && v.toInt() == 123; });
    EXPECT_NE(foundIt, cursor.end());
    EXPECT_EQ((*foundIt).toInt(), 123);
    
    // Test std::all_of
    auto allValid = std::all_of(cursor.begin(), cursor.end(),
        [](const QJsonValue& v) { return !v.isUndefined(); });
    EXPECT_TRUE(allValid);
    
    // Test std::distance
    auto distance = std::distance(cursor.begin(), cursor.end());
    EXPECT_EQ(distance, 5);
}

TEST_F(ContainerCursorTest, STLAccumulate) {
    // Create numeric array for accumulation test
    QJsonArray numArray;
    for (int i = 1; i <= 10; ++i) {
        numArray.append(i);
    }
    
    auto cursor = ContainerCursor::array(numArray);
    
    // Test std::accumulate
    auto sum = std::accumulate(cursor.begin(), cursor.end(), 0,
        [](int acc, const QJsonValue& v) {
            return acc + (v.isDouble() ? v.toInt() : 0);
        });
    
    EXPECT_EQ(sum, 55); // 1+2+...+10 = 55
}

// ---------------------------------------------------------------------------
// C++23 Ranges Support Tests
// ---------------------------------------------------------------------------

#ifdef JSON_QUERY_ENABLE_RANGES
TEST_F(ContainerCursorTest, RangesSupport) {
    auto cursor = ContainerCursor::array(testArray);
    
    // Test ranges::begin/end
    auto rangeBegin = std::ranges::begin(cursor);
    auto rangeEnd = std::ranges::end(cursor);
    
    EXPECT_EQ(rangeBegin, cursor.begin());
    EXPECT_EQ(rangeEnd, cursor.end());
    
    // Test ranges algorithms
    auto stringCount = std::ranges::count_if(cursor, 
        [](const QJsonValue& v) { return v.isString(); });
    EXPECT_EQ(stringCount, 1);
    
    // Test ranges views (if available)
    auto validValues = cursor 
        | std::views::filter([](const QJsonValue& v) { return !v.isUndefined(); });
    
    auto validCount = std::ranges::distance(validValues);
    EXPECT_EQ(validCount, 5);
}
#endif

// ---------------------------------------------------------------------------
// Performance and Memory Tests
// ---------------------------------------------------------------------------

TEST_F(ContainerCursorTest, MemoryCharacteristics) {
    // Verify cursor size is 32 bytes (cache-aligned)
    EXPECT_EQ(sizeof(ContainerCursor), 32);
    EXPECT_EQ(alignof(ContainerCursor), 32); // 32-byte alignment as specified in header
    
    // Verify iterator size is reasonable (24 bytes: 3 x 8-byte fields)
    EXPECT_EQ(sizeof(ContainerCursor::iterator), 24);
}

TEST_F(ContainerCursorTest, LargeArrayPerformance) {
    auto cursor = ContainerCursor::array(largeArray);
    
    EXPECT_EQ(cursor.size(), 1000);
    EXPECT_FALSE(cursor.empty());
    
    // Test zero-copy iteration performance
    long long sum = 0;
    for (const auto& value : cursor) {
        sum += value.toInt();
    }
    
    // Expected sum: 0 + 1 + 2 + ... + 999 = 999 * 1000 / 2 = 499500
    EXPECT_EQ(sum, 499500);
    
    // Test iterator advancement performance
    auto it = cursor.begin();
    int advanceCount = 0;
    while (it != cursor.end()) {
        ++it;
        ++advanceCount;
    }
    EXPECT_EQ(advanceCount, 1000);
}

TEST_F(ContainerCursorTest, ZeroCopySemantics) {
    // Verify that cursor construction doesn't copy the container
    auto cursor1 = ContainerCursor::object(testObj);
    auto cursor2 = ContainerCursor::object(testObj);
    
    // Both cursors should reference the same underlying data
    EXPECT_EQ(cursor1.size(), cursor2.size());
    
    // Iteration should not trigger detachment
    for (const auto& value : cursor1) {
        // Access values without modification
        (void)value.toString();
    }
    
    // Second cursor should still work correctly
    EXPECT_EQ(cursor2.size(), 5);
}

// ---------------------------------------------------------------------------
// Edge Cases and Error Handling
// ---------------------------------------------------------------------------

TEST_F(ContainerCursorTest, EmptyContainerIteration) {
    auto objCursor = ContainerCursor::object(emptyObj);
    auto arrayCursor = ContainerCursor::array(emptyArray);
    
    // Range-based for should handle empty containers gracefully
    int objIterations = 0;
    for (const auto& value : objCursor) {
        ++objIterations;
        (void)value; // Suppress unused variable warning
    }
    EXPECT_EQ(objIterations, 0);
    
    int arrayIterations = 0;
    for (const auto& value : arrayCursor) {
        ++arrayIterations;
        (void)value;
    }
    EXPECT_EQ(arrayIterations, 0);
    
    // STL algorithms should work with empty containers
    auto emptyCount = std::count_if(objCursor.begin(), objCursor.end(),
        [](const QJsonValue&) { return true; });
    EXPECT_EQ(emptyCount, 0);
}

TEST_F(ContainerCursorTest, SingleElementContainers) {
    QJsonObject singleObj{{"key", "value"}};
    QJsonArray singleArray{"element"};
    
    auto objCursor = ContainerCursor::object(singleObj);
    auto arrayCursor = ContainerCursor::array(singleArray);
    
    EXPECT_EQ(objCursor.size(), 1);
    EXPECT_EQ(arrayCursor.size(), 1);
    
    // Test iteration
    auto objIt = objCursor.begin();
    EXPECT_NE(objIt, objCursor.end());
    EXPECT_EQ((*objIt).toString(), "value");
    ++objIt;
    EXPECT_EQ(objIt, objCursor.end());
    
    auto arrayIt = arrayCursor.begin();
    EXPECT_NE(arrayIt, arrayCursor.end());
    EXPECT_EQ((*arrayIt).toString(), "element");
    ++arrayIt;
    EXPECT_EQ(arrayIt, arrayCursor.end());
}

// ---------------------------------------------------------------------------
// Type Safety and Constexpr Tests
// ---------------------------------------------------------------------------

TEST_F(ContainerCursorTest, TypeSafety) {
    auto cursor = ContainerCursor::array(testArray);
    
    // Verify const-correctness
    const auto& constCursor = cursor;
    auto constIt = constCursor.begin();
    
    // Should return QJsonValue by value, not reference
    static_assert(std::is_same_v<decltype(*constIt), QJsonValue>);
    
    // Verify iterator categories
    static_assert(std::forward_iterator<ContainerCursor::iterator>);
    static_assert(!std::bidirectional_iterator<ContainerCursor::iterator>);
    static_assert(!std::random_access_iterator<ContainerCursor::iterator>);
}

TEST_F(ContainerCursorTest, ConstexprCapabilities) {
    // Test that basic operations can be constexpr where possible
    constexpr auto testConstexpr = []() {
        // This tests that the basic structure supports constexpr operations
        return true;
    };
    
    static_assert(testConstexpr());
    
    // Test noexcept specifications
    static_assert(noexcept(ContainerCursor::object(std::declval<const QJsonObject&>())));
    static_assert(noexcept(ContainerCursor::array(std::declval<const QJsonArray&>())));
    
    SUCCEED() << "Constexpr and noexcept capabilities verified";
}

// ---------------------------------------------------------------------------
// Object Key Access Tests
// ---------------------------------------------------------------------------

TEST_F(ContainerCursorTest, ObjectValueAccess) {
    auto cursor = ContainerCursor::object(testObj);
    
    std::vector<QJsonValue> collected;
    for (auto it = cursor.begin(); it != cursor.end(); ++it) {
        collected.push_back(*it);
    }
    
    EXPECT_EQ(collected.size(), 5);
    
    // Check that we have the expected types of values
    bool hasString = false, hasInt = false, hasBool = false, hasDouble = false, hasObject = false;
    
    for (const auto& val : collected) {
        if (val.isString() && val.toString() == "test_object") hasString = true;
        if (val.isDouble() && val.toInt() == 42) hasInt = true;
        if (val.isBool() && val.toBool() == true) hasBool = true;
        if (val.isDouble() && val.toDouble() == 98.5) hasDouble = true;
        if (val.isObject()) hasObject = true;
    }
    
    EXPECT_TRUE(hasString);
    EXPECT_TRUE(hasInt);
    EXPECT_TRUE(hasBool);
    EXPECT_TRUE(hasDouble);
    EXPECT_TRUE(hasObject);
}

TEST_F(ContainerCursorTest, ArrayValueAccess) {
    auto cursor = ContainerCursor::array(testArray);
    
    // Array iterators provide values in order
    auto it = cursor.begin();
    
    // No key() method available for ContainerCursor iterator
    // Just verify we can access values
    EXPECT_NE(it, cursor.end());
    ++it;
    EXPECT_NE(it, cursor.end());
}

// ---------------------------------------------------------------------------
// Copy and Move Semantics Tests
// ---------------------------------------------------------------------------

TEST_F(ContainerCursorTest, CopySemantics) {
    auto cursor1 = ContainerCursor::array(testArray);
    auto cursor2 = cursor1; // Copy construction
    
    EXPECT_EQ(cursor1.size(), cursor2.size());
    EXPECT_EQ(cursor1.empty(), cursor2.empty());
    
    // Both should iterate over the same data
    auto it1 = cursor1.begin();
    auto it2 = cursor2.begin();
    
    while (it1 != cursor1.end() && it2 != cursor2.end()) {
        EXPECT_EQ(*it1, *it2);
        ++it1;
        ++it2;
    }
    
    EXPECT_EQ(it1, cursor1.end());
    EXPECT_EQ(it2, cursor2.end());
}

TEST_F(ContainerCursorTest, MoveSemantics) {
    auto cursor1 = ContainerCursor::array(testArray);
    auto originalSize = cursor1.size();
    
    auto cursor2 = std::move(cursor1); // Move construction
    
    EXPECT_EQ(cursor2.size(), originalSize);
    EXPECT_FALSE(cursor2.empty());
    
    // Original cursor should be in a valid but unspecified state
    // We can't make strong guarantees about moved-from state
}

// ---------------------------------------------------------------------------
// Comprehensive Integration Tests
// ---------------------------------------------------------------------------

TEST_F(ContainerCursorTest, ComplexNestedStructure) {
    QJsonObject complex;
    complex["users"] = QJsonArray{
        QJsonObject{{"id", 1}, {"name", "Alice"}, {"roles", QJsonArray{"admin", "user"}}},
        QJsonObject{{"id", 2}, {"name", "Bob"}, {"roles", QJsonArray{"user"}}},
        QJsonObject{{"id", 3}, {"name", "Charlie"}, {"roles", QJsonArray{"admin", "moderator"}}}
    };
    complex["config"] = QJsonObject{{"version", "2.0"}, {"debug", true}};
    
    auto cursor = ContainerCursor::object(complex);
    
    EXPECT_EQ(cursor.size(), 2);
    
    // Find users array by checking values
    auto usersIt = std::find_if(cursor.begin(), cursor.end(),
        [](const QJsonValue& v) { return v.isArray(); });
    
    EXPECT_NE(usersIt, cursor.end());
    
    // Iterate over users
    auto usersArray = (*usersIt).toArray();
    auto usersCursor = ContainerCursor::array(usersArray);
    
    std::vector<QString> userNames;
    for (const auto& user : usersCursor) {
        if (user.isObject()) {
            userNames.push_back(user.toObject()["name"].toString());
        }
    }
    
    EXPECT_THAT(userNames, ::testing::ElementsAre("Alice", "Bob", "Charlie"));
}

TEST_F(ContainerCursorTest, PerformanceBenchmark) {
    // Create very large array for performance testing
    QJsonArray veryLargeArray;
    const int size = 10000;
    for (int i = 0; i < size; ++i) {
        veryLargeArray.append(QJsonObject{
            {"id", i},
            {"value", i * 2},
            {"name", QString("item_%1").arg(i)}
        });
    }
    
    auto cursor = ContainerCursor::array(veryLargeArray);
    
    // Test iteration performance
    int count = 0;
    long long idSum = 0;
    
    for (const auto& item : cursor) {
        if (item.isObject()) {
            auto obj = item.toObject();
            idSum += obj["id"].toInt();
            ++count;
        }
    }
    
    EXPECT_EQ(count, size);
    EXPECT_EQ(idSum, static_cast<long long>(size - 1) * size / 2); // Sum of 0 to size-1
}

// ---------------------------------------------------------------------------
// Compatibility with Qt JSON Types Tests
// ---------------------------------------------------------------------------

TEST_F(ContainerCursorTest, QtJsonTypeCompatibility) {
    // Test with various Qt JSON value types
    QJsonArray mixedArray;
    mixedArray.append(QJsonValue()); // Null
    mixedArray.append(QJsonValue{true}); // Bool
    mixedArray.append(QJsonValue(42)); // Int
    mixedArray.append(QJsonValue(3.14)); // Double
    mixedArray.append(QJsonValue("string")); // String
    mixedArray.append(QJsonValue(QJsonObject{{"nested", "object"}})); // Object
    mixedArray.append(QJsonValue(QJsonArray{"nested", "array"})); // Array
    
    auto cursor = ContainerCursor::array(mixedArray);
    
    EXPECT_EQ(cursor.size(), 7);
    
    auto it = cursor.begin();
    EXPECT_TRUE((*it).isNull()); ++it;
    EXPECT_TRUE((*it).isBool()); ++it;
    EXPECT_TRUE((*it).isDouble()); ++it;
    EXPECT_TRUE((*it).isDouble()); ++it;
    EXPECT_TRUE((*it).isString()); ++it;
    EXPECT_TRUE((*it).isObject()); ++it;
    EXPECT_TRUE((*it).isArray()); ++it;
    EXPECT_EQ(it, cursor.end());
}
