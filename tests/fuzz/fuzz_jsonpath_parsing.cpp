/**
 * @file fuzz_jsonpath_parsing.cpp
 * @brief LibFuzzer target for JSONPath expression parsing
 *
 * This fuzzer tests the robustness of JSONPath::create() with malformed,
 * edge case, and randomly generated JSONPath expressions.
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

    // Convert raw bytes to QString for JSONPath parsing
    QByteArray input(reinterpret_cast<const char*>(data), static_cast<int>(size));
    QString    jsonPathExpression = QString::fromUtf8(input);

    // Test JSONPath parsing robustness
    auto pathResult = JSONPath::create(jsonPathExpression);

    if (pathResult)
    {
        // If parsing succeeded, test with a simple JSON document
        // This ensures the parsed path can be evaluated without crashing
        QJsonObject testDoc{{"store",
                             QJsonObject{{"book",
                                          QJsonArray{QJsonObject{{"title", "Book 1"}, {"price", 10.99}},
                                                     QJsonObject{{"title", "Book 2"}, {"price", 8.95}}}},
                                         {"bicycle", QJsonObject{{"color", "red"}, {"price", 19.95}}}}},
                            {"expensive", 10}};

        QJsonDocument doc(testDoc);

        // Test evaluation - should not crash even with complex expressions
        auto evalResult = pathResult->evaluate(doc);

        // The result can be success or failure, but should not crash
        // std::expected handles errors gracefully
        static_cast<void>(evalResult); // Suppress unused variable warning
    }

    // Test completed without crashes
    return 0;
}
