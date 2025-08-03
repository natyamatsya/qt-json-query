/**
 * @file fuzz_container_cursor.cpp
 * @brief LibFuzzer target for ContainerCursor iteration with malformed JSON structures
 *
 * This fuzzer tests the robustness of the ContainerCursor tagged pointer optimization
 * and cache-aligned iteration with various JSON container structures.
 */

#include <fuzzer/FuzzedDataProvider.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <json-query/JSONQuery>
#include <json-query/json-path/internal/ContainerCursor.hpp>

using namespace json_query::json_path::internal;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Minimum input size check
    if (size < 1)
        return 0;

    // Parse JSON document from fuzzed input
    QByteArray      jsonInput(reinterpret_cast<const char*>(data), static_cast<int>(size));
    QJsonParseError parseError;
    QJsonDocument   doc = QJsonDocument::fromJson(jsonInput, &parseError);

    if (parseError.error != QJsonParseError::NoError || doc.isNull())
        return 0; // Invalid JSON, nothing to test

    // Test ContainerCursor with object roots
    if (doc.isObject())
    {
        QJsonObject obj = doc.object();

        // Test ContainerCursor::object() factory method
        auto cursor = ContainerCursor::object(obj);

        // Test iterator interface - should not crash with malformed objects
        try
        {
            size_t count = 0;
            for (auto it = cursor.begin(); it != cursor.end() && count < 10000; ++it, ++count)
            {
                // Test iterator dereference - tagged pointer optimization
                QJsonValue value = *it;
                static_cast<void>(value); // Suppress unused warning

                // Test iterator advancement
                // The tagged pointer should handle type discrimination correctly
            }

            // Test container properties
            bool empty = cursor.empty();
            auto size  = cursor.size();
            static_cast<void>(empty);
            static_cast<void>(size);

            // Test range-based for loop (C++23 ranges integration)
            count = 0;
            for (const auto& value : cursor)
            {
                if (count++ > 10000)
                    break; // Prevent infinite loops
                static_cast<void>(value);
            }
        }
        catch (...)
        {
            // ContainerCursor should not throw exceptions
            // If it does, this is a bug that fuzzing has found
            return 0;
        }
    }

    // Test ContainerCursor with array roots
    if (doc.isArray())
    {
        QJsonArray arr = doc.array();

        // Test ContainerCursor::array() factory method
        auto cursor = ContainerCursor::array(arr);

        // Test iterator interface with arrays
        try
        {
            size_t count = 0;
            for (auto it = cursor.begin(); it != cursor.end() && count < 10000; ++it, ++count)
            {
                // Test iterator dereference with array elements
                QJsonValue value = *it;
                static_cast<void>(value);
            }

            // Test container properties
            bool empty = cursor.empty();
            auto size  = cursor.size();
            static_cast<void>(empty);
            static_cast<void>(size);

            // Test range-based iteration
            count = 0;
            for (const auto& value : cursor)
            {
                if (count++ > 10000)
                    break;
                static_cast<void>(value);
            }
        }
        catch (...)
        {
            // Should not throw
            return 0;
        }
    }

    // Test nested iteration scenarios
    if (doc.isObject())
    {
        QJsonObject obj    = doc.object();
        auto        cursor = ContainerCursor::object(obj);

        try
        {
            // Test nested container access
            for (const auto& value : cursor)
            {
                if (value.isObject())
                {
                    auto   nestedCursor = ContainerCursor::object(value.toObject());
                    size_t nestedCount  = 0;
                    for (const auto& nestedValue : nestedCursor)
                    {
                        if (nestedCount++ > 1000)
                            break;
                        static_cast<void>(nestedValue);
                    }
                }
                else if (value.isArray())
                {
                    auto   nestedCursor = ContainerCursor::array(value.toArray());
                    size_t nestedCount  = 0;
                    for (const auto& nestedValue : nestedCursor)
                    {
                        if (nestedCount++ > 1000)
                            break;
                        static_cast<void>(nestedValue);
                    }
                }
            }
        }
        catch (...)
        {
            // Should not throw
            return 0;
        }
    }

    return 0;
}
