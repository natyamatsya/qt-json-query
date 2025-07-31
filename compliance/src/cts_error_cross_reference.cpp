// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/JSONQuery"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>

#include <iostream>
#include <fstream>

using namespace json_query;

struct TestCase
{
    QString    name;
    QString    selector;
    bool       invalid_selector{false};
    QJsonValue document;
    QJsonArray expected_result;
};

void analyzeTestCase(const TestCase& testCase)
{
    std::cout << "\n=== " << testCase.name.toStdString() << " ===" << std::endl;
    std::cout << "Selector: " << testCase.selector.toStdString() << std::endl;
    std::cout << "Expected invalid: " << (testCase.invalid_selector ? "YES" : "NO") << std::endl;

    auto pathResult = JSONPath::create(testCase.selector);
    if (!pathResult)
    {
        std::cout << "✅ COMPILATION ERROR: " << toQStringView(pathResult.error()).toString().toStdString()
                  << " (code: " << static_cast<int>(pathResult.error().code) << ")" << std::endl;
        if (!testCase.invalid_selector)
            std::cout << "⚠️  WARNING: Expected valid selector but got compilation error!" << std::endl;
        return;
    }

    if (testCase.invalid_selector)
    {
        std::cout << "⚠️  WARNING: Expected invalid selector but compilation succeeded!" << std::endl;
        return;
    }

    // Test evaluation if we have a document
    if (!testCase.document.isNull())
    {
        auto evalResult{pathResult->evaluateAll(testCase.document)};
        if (!evalResult)
        {
            std::cout << "❌ EVALUATION ERROR: " << toQStringView(evalResult.error()).toString().toStdString()
                      << " (code: " << static_cast<int>(evalResult.error().code) << ")" << std::endl;
        }
        else
        {
            std::cout << "✅ SUCCESS: " << evalResult->size() << " results" << std::endl;
            std::cout << "Expected: " << testCase.expected_result.size() << " results" << std::endl;
            if (evalResult->size() != testCase.expected_result.size())
                std::cout << "⚠️  MISMATCH: Result count differs from expected!" << std::endl;
        }
    }
}

int main()
{
    std::cout << "=== CTS Error Cross-Reference Analysis ===" << std::endl;

    // Manually analyze key error scenarios from CTS
    std::vector<TestCase> testCases = {
        // Invalid selector cases (compilation errors)
        TestCase{"no leading whitespace", " $", true, QJsonValue(), QJsonArray()},
        TestCase{"no trailing whitespace", "$ ", true, QJsonValue(), QJsonArray()},
        TestCase{"name shorthand, symbol", "$.&", true, QJsonValue(), QJsonArray()},
        TestCase{"name shorthand, number", "$.1", true, QJsonValue(), QJsonArray()},

        // Valid selectors with empty results (evaluation scenarios)
        TestCase{"name shorthand, absent data", "$.c", false, QJsonObject{{"a", "A"}, {"b", "B"}}, QJsonArray()},

        // Index out of bounds scenarios
        TestCase{"out of bound", "$[2]", false, QJsonArray{"first", "second"}, QJsonArray()},
        TestCase{"negative out of bound", "$[-3]", false, QJsonArray{"first", "second"}, QJsonArray()},

        // Type mismatch scenarios
        TestCase{"index on object", "$[0]", false, QJsonObject{{"key", "value"}}, QJsonArray()},
        TestCase{"key on array", "$.key", false, QJsonArray{"first", "second"}, QJsonArray()},

        // Slice scenarios
        TestCase{"slice on object", "$[0:2]", false, QJsonObject{{"key", "value"}}, QJsonArray()},
        TestCase{"zero step slice", "$[0:2:0]", false, QJsonArray{"a", "b", "c"}, QJsonArray()},
    };

    for (const auto& testCase : testCases)
        analyzeTestCase(testCase);

    std::cout << "\n=== Analysis Complete ===" << std::endl;
    return 0;
}
