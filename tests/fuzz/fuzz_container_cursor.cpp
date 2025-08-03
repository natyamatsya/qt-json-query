/**
 * @file fuzz_container_cursor.cpp
 * @brief LibFuzzer target for JSON container iteration robustness
 *
 * This fuzzer tests the robustness of JSON container iteration through
 * the public JSONPath API with various malformed JSON structures.
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

    // Parse JSON document from fuzzed input
    QByteArray      jsonInput(reinterpret_cast<const char*>(data), static_cast<int>(size));
    QJsonParseError parseError;
    QJsonDocument   doc = QJsonDocument::fromJson(jsonInput, &parseError);

    if (parseError.error != QJsonParseError::NoError || doc.isNull())
        return 0; // Invalid JSON, nothing to test

    // Test various JSONPath expressions that exercise container iteration
    QStringList testExpressions = {
        "$.*",           // All children (object/array iteration)
        "$..*",          // Recursive descent (nested iteration)
        "$[*]",          // Array wildcard
        "$['*']",        // Object wildcard with quotes
        "$[0,1,2]",      // Multiple array indices
        "$['a','b','c']" // Multiple object keys
    };

    for (const QString& expression : testExpressions)
    {
        try
        {
            auto pathResult = JSONPath::create(expression);
            if (pathResult)
            {
                // Test evaluation which internally uses container iteration
                auto result = pathResult->evaluate(doc);
                static_cast<void>(result); // Suppress unused warning
            }
        }
        catch (...)
        {
            // Should not throw, but if it does, continue testing
        }
    }

    // Test with nested structures to exercise deep iteration
    if (doc.isObject())
    {
        QJsonObject obj = doc.object();

        // Test recursive patterns that stress container iteration
        QStringList recursiveExpressions = {"$..*", "$..book", "$..book[*]", "$..book[*].title"};

        for (const QString& expression : recursiveExpressions)
        {
            try
            {
                auto pathResult = JSONPath::create(expression);
                if (pathResult)
                {
                    auto result = pathResult->evaluate(doc);
                    static_cast<void>(result);
                }
            }
            catch (...)
            {
                // Continue testing
            }
        }
    }

    return 0;
}
