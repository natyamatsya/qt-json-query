/**
 * fuzz_simple_api.cpp - Simple API fuzzing for qt-json-query
 *
 * This fuzzer tests the public API of JSONPath and JSONPointer classes
 * with randomly generated inputs, focusing on the main entry points.
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

// Simple test JSON document
static const QJsonObject createTestDocument()
{
    QJsonObject root;
    root["store"]     = QJsonObject{{"book",
                                     QJsonArray{QJsonObject{{"title", "Book 1"}, {"price", 10.99}},
                                            QJsonObject{{"title", "Book 2"}, {"price", 15.99}}}},
                                    {"bicycle", QJsonObject{{"color", "red"}, {"price", 19.95}}}};
    root["expensive"] = 10;
    return root;
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 1)
        return 0;

    FuzzedDataProvider fdp(data, size);

    // Get a fuzzed string for JSONPath/JSONPointer expression
    std::string expression = fdp.ConsumeRandomLengthString(200);
    if (expression.empty())
        return 0;

    // Test document
    QJsonObject testDoc = createTestDocument();

    // Decide whether to test JSONPath or JSONPointer
    bool testJSONPath = fdp.ConsumeBool();

    try
    {
        if (testJSONPath)
        {
            // Test JSONPath parsing and evaluation
            auto pathResult = JSONPath::create(QString::fromStdString(expression));
            if (pathResult)
            {
                // If parsing succeeded, try evaluation
                try
                {
                    auto result = pathResult->evaluate(testDoc);
                    if (result)
                    {
                        // Successfully evaluated, result contains QJsonValue
                        (void)result.value(); // Suppress unused variable warning
                    }
                }
                catch (...)
                {
                    // Evaluation can fail, that's fine
                }
            }
        }
        else
        {
            // Test JSONPointer parsing and evaluation
            auto pointerResult = JSONPointer::create(QString::fromStdString(expression));
            if (pointerResult)
            {
                // If parsing succeeded, try evaluation
                try
                {
                    auto result = pointerResult->evaluate(testDoc);
                    if (result)
                    {
                        // Successfully evaluated, result contains QJsonValue
                        (void)result.value(); // Suppress unused variable warning
                    }
                }
                catch (...)
                {
                    // Evaluation can fail, that's fine
                }
            }
        }
    }
    catch (...)
    {
        // Any exception is fine - we're testing robustness
    }

    return 0;
}
