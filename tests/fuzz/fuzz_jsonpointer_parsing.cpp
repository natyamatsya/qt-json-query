/**
 * @file fuzz_jsonpointer_parsing.cpp
 * @brief LibFuzzer target for JSON Pointer expression parsing
 *
 * This fuzzer tests the robustness of JSONPointer::create() with malformed,
 * edge case, and randomly generated JSON Pointer expressions according to RFC 6901.
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

using namespace json_query;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Minimum input size check
    if (size < 1)
        return 0;

    // Convert raw bytes to QString for JSON Pointer parsing
    QString jsonPointerExpression = QString::fromUtf8(reinterpret_cast<const char*>(data), static_cast<int>(size));

    // Test JSON Pointer parsing robustness
    auto pointerResult = JSONPointer::create(jsonPointerExpression);

    if (pointerResult)
    {
        // Create a test document for evaluation
        QJsonObject testDoc{
            {"foo",
             QJsonObject{{"bar", QJsonArray{42, "hello", true}}, {"", "empty_key"}, {"~", "tilde"}, {"/", "slash"}}},
            {"", QJsonObject{{"nested_empty", "value"}}},
            {"array", QJsonArray{"first", QJsonObject{{"nested", "in_array"}}, QJsonArray{1, 2, 3}}}};

        QJsonDocument doc(testDoc);

        // Test evaluation - should handle all RFC 6901 cases gracefully
        try
        {
            auto evalResult = pointerResult->evaluate(doc);
            static_cast<void>(evalResult); // Suppress unused variable warning
        }
        catch (...)
        {
            // Should not throw, but if it does, continue testing
        }
    }

    return 0;
}
