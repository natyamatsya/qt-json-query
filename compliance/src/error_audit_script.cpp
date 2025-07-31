// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/JSONQuery"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include <iostream>

using namespace json_query;

void testErrorScenario(const QString& description, const QString& jsonPath, const QJsonValue& document)
{
    std::cout << "\n=== " << description.toStdString() << " ===" << std::endl;
    std::cout << "JSONPath: " << jsonPath.toStdString() << std::endl;

    // Convert QJsonValue to JSON string for display
    QJsonDocument doc;
    if (document.isObject())
    {
        doc = QJsonDocument(document.toObject());
    }
    else if (document.isArray())
    {
        doc = QJsonDocument(document.toArray());
    }
    else
    {
        // For primitive values, wrap in an array for display
        QJsonArray wrapper;
        wrapper.append(document);
        doc = QJsonDocument(wrapper);
    }
    std::cout << "Document: " << doc.toJson(QJsonDocument::Compact).toStdString() << std::endl;

    auto pathResult = JSONPath::create(jsonPath);
    if (!pathResult)
    {
        std::cout << "Compilation Error: " << to_std_sv(pathResult.error())
                  << " (code: " << static_cast<int>(pathResult.error().code) << ")" << std::endl;
        return;
    }

    auto evalResult{pathResult->evaluateAll(document)};
    if (!evalResult)
    {
        std::cout << "Evaluation Error: " << to_std_sv(evalResult.error())
                  << " (code: " << static_cast<int>(evalResult.error().code) << ")" << std::endl;
    }
    else
    {
        std::cout << "Success: " << evalResult->size() << " results" << std::endl;
        for (const auto& result : *evalResult)
        {
            QJsonDocument resultDoc;
            if (result.isObject())
            {
                resultDoc = QJsonDocument(result.toObject());
            }
            else if (result.isArray())
            {
                resultDoc = QJsonDocument(result.toArray());
            }
            else
            {
                QJsonArray wrapper;
                wrapper.append(result);
                resultDoc = QJsonDocument(wrapper);
            }
            std::cout << "  Result: " << resultDoc.toJson(QJsonDocument::Compact).toStdString() << std::endl;
        }
    }
}

int main()
{
    std::cout << "=== RFC 9535 Error Code Compliance Audit ===" << std::endl;

    // Test data
    QJsonObject emptyObject;
    QJsonArray  emptyArray;
    QJsonArray  smallArray;
    smallArray.append("a");
    smallArray.append("b");

    QJsonObject simpleObject;
    simpleObject["key"]    = "value";
    simpleObject["number"] = 42;

    // 1. Index Out of Range Scenarios
    std::cout << "\n### INDEX OUT OF RANGE SCENARIOS ###" << std::endl;
    testErrorScenario("Positive index out of bounds", "$[5]", smallArray);
    testErrorScenario("Negative index out of bounds", "$[-5]", smallArray);
    testErrorScenario("Index on empty array", "$[0]", emptyArray);

    // 2. Type Mismatch Array Scenarios
    std::cout << "\n### TYPE MISMATCH ARRAY SCENARIOS ###" << std::endl;
    testErrorScenario("Index access on object", "$[0]", simpleObject);
    testErrorScenario("Index access on string", "$[0]", QJsonValue("hello"));
    testErrorScenario("Index access on number", "$[0]", QJsonValue(42));
    testErrorScenario("Index access on null", "$[0]", QJsonValue());

    // 3. Type Mismatch Object Scenarios
    std::cout << "\n### TYPE MISMATCH OBJECT SCENARIOS ###" << std::endl;
    testErrorScenario("Key access on array", "$.key", smallArray);
    testErrorScenario("Key access on string", "$.key", QJsonValue("hello"));
    testErrorScenario("Key access on number", "$.key", QJsonValue(42));
    testErrorScenario("Key access on null", "$.key", QJsonValue());

    // 4. Key Not Found Scenarios
    std::cout << "\n### KEY NOT FOUND SCENARIOS ###" << std::endl;
    testErrorScenario("Missing key", "$.missing", simpleObject);
    testErrorScenario("Missing nested key", "$.key.missing", simpleObject);

    // 5. Invalid Slice Scenarios
    std::cout << "\n### INVALID SLICE SCENARIOS ###" << std::endl;
    testErrorScenario("Zero step slice", "$[0:2:0]", smallArray);
    testErrorScenario("Slice on object", "$[0:2]", simpleObject);

    // 6. Compilation Error Scenarios
    std::cout << "\n### COMPILATION ERROR SCENARIOS ###" << std::endl;
    testErrorScenario("Missing root", "[0]", smallArray);
    testErrorScenario("Trailing dot", "$.", smallArray);
    testErrorScenario("Unmatched bracket", "$[0", smallArray);
    testErrorScenario("Invalid identifier", "$.123invalid", simpleObject);

    return 0;
}
