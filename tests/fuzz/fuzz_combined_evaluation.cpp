/**
 * @file fuzz_combined_evaluation.cpp
 * @brief LibFuzzer target for combined JSONPath expression and JSON document evaluation
 *
 * This fuzzer tests the robustness of the complete evaluation pipeline by fuzzing
 * both the JSONPath expression and the JSON document simultaneously.
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
#include <QByteArray>
#include <QString>
#include <QJsonParseError>
#include <algorithm>

using namespace json_query;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    // Minimum input size check
    if (size < 2)
        return 0;

    // Split input: first byte determines split point
    size_t splitPoint = data[0] % (size - 1) + 1;

    // First part: JSONPath expression
    QByteArray pathInput(reinterpret_cast<const char*>(data + 1), static_cast<int>(std::min(splitPoint, size - 1)));
    QString    jsonPathExpression = QString::fromUtf8(pathInput);

    // Second part: JSON document
    QByteArray jsonInput(reinterpret_cast<const char*>(data + splitPoint + 1),
                         static_cast<int>(size - splitPoint - 1));

    // Test JSONPath parsing
    auto pathResult = JSONPath::create(jsonPathExpression);

    if (pathResult)
    {
        // Test JSON document parsing
        QJsonParseError parseError;
        QJsonDocument   doc = QJsonDocument::fromJson(jsonInput, &parseError);

        if (parseError.error == QJsonParseError::NoError && !doc.isNull())
        {
            // Both JSONPath and JSON are valid - test evaluation
            auto evalResult = pathResult->evaluateSingle(doc);

            // Test both evaluateSingle() and evaluate() if available
            static_cast<void>(evalResult);

            // Test with different document root types
            if (doc.isObject())
            {
                // Test object root evaluation
                auto objResult = pathResult->evaluateSingle(doc);
                static_cast<void>(objResult);
            }
            else if (doc.isArray())
            {
                // Test array root evaluation
                auto arrResult = pathResult->evaluate(doc);
                static_cast<void>(arrResult);
            }
        }

        // Also test with some known-good JSON structures to ensure
        // the path evaluation doesn't crash on complex expressions
        QJsonDocument knownGoodDoc = QJsonDocument::fromJson(R"({
            "store": {
                "book": [
                    {"category": "reference", "author": "Nigel Rees", "title": "Sayings of the Century", "price": 8.95},
                    {"category": "fiction", "author": "Evelyn Waugh", "title": "Sword of Honour", "price": 12.99},
                    {"category": "fiction", "author": "Herman Melville", "title": "Moby Dick", "isbn": "0-553-21311-3", "price": 8.99},
                    {"category": "fiction", "author": "J. R. R. Tolkien", "title": "The Lord of the Rings", "isbn": "0-395-19395-8", "price": 22.99}
                ],
                "bicycle": {"color": "red", "price": 19.95}
            },
            "expensive": 10
        })");

        if (!knownGoodDoc.isNull())
        {
            auto knownGoodResult = pathResult->evaluate(knownGoodDoc);
            static_cast<void>(knownGoodResult);
        }
    }

    // Test completed without crashes
    return 0;
}
