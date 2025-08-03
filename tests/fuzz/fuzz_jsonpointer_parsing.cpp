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
#include <QString>
#include <QByteArray>

using namespace json_query;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Minimum input size check
    if (size < 1)
        return 0;

    // Convert raw bytes to QString for JSON Pointer parsing
    QByteArray input(reinterpret_cast<const char*>(data), static_cast<int>(size));
    QString    jsonPointerExpression = QString::fromUtf8(input);

    // Test JSON Pointer parsing robustness
    auto pointerResult = JSONPointer::create(jsonPointerExpression);

    if (pointerResult)
    {
        // If parsing succeeded, test with various JSON document structures
        // This tests RFC 6901 compliance and edge cases

        // Test with nested object structure
        QJsonObject nestedDoc{
            {"foo",
             QJsonObject{
                 {"bar", QJsonArray{42, "hello", true}},
                 {"baz", QJsonObject{{"qux", "world"}}},
                 {"", "empty_key"}, // Empty key edge case
                 {"~", "tilde"},    // Tilde escape sequence test
                 {"/", "slash"}     // Slash escape sequence test
             }},
            {"", QJsonObject{{"nested_empty", "value"}}}, // Empty key at root
            {"array", QJsonArray{"first", QJsonObject{{"nested", "in_array"}}, QJsonArray{1, 2, 3}}}};

        QJsonDocument doc(nestedDoc);

        // Test evaluation - should handle all RFC 6901 cases gracefully
        auto evalResult = pointerResult->evaluate(doc);

        // The result can be success or failure, but should not crash
        // std::expected handles errors like KeyNotFound, IndexOutOfBounds gracefully
        static_cast<void>(evalResult); // Suppress unused variable warning

        // Test with array-heavy document for index boundary testing
        QJsonArray arrayDoc;
        for (int i = 0; i < 100; ++i)
            arrayDoc.append(QJsonObject{{"index", i}, {"value", QString("item_%1").arg(i)}});

        QJsonDocument arrayDocument(arrayDoc);
        auto          arrayEvalResult = pointerResult->evaluate(arrayDocument);
        static_cast<void>(arrayEvalResult);
    }

    // Test completed without crashes
    return 0;
}
