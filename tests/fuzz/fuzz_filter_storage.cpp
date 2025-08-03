/**
 * @file fuzz_filter_storage.cpp
 * @brief LibFuzzer target for JSONPath filter expression robustness
 *
 * This fuzzer tests the robustness of JSONPath filter expressions and
 * evaluation through the public API with various filter conditions.
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
    if (size < 2)
        return 0;

    FuzzedDataProvider fdp(data, size);

    // Split input between JSONPath expression and JSON document
    size_t               pathSize  = fdp.ConsumeIntegralInRange<size_t>(1, size / 2);
    std::vector<uint8_t> pathBytes = fdp.ConsumeBytes<uint8_t>(pathSize);
    std::string          pathData(pathBytes.begin(), pathBytes.end());
    std::string          jsonData = fdp.ConsumeRemainingBytesAsString();

    if (pathData.empty() || jsonData.empty())
        return 0;

    // Parse JSON document
    QByteArray      jsonInput(jsonData.c_str(), static_cast<int>(jsonData.size()));
    QJsonParseError parseError;
    QJsonDocument   doc = QJsonDocument::fromJson(jsonInput, &parseError);

    if (parseError.error != QJsonParseError::NoError || doc.isNull())
        return 0;

    // Test various filter expressions with the fuzzed path data
    QString basePath = QString::fromUtf8(pathData.c_str());

    // Generate filter expressions that exercise filter storage systems
    QStringList filterExpressions = {
        QString("$[?(@.%1)]").arg(basePath),                  // Simple property filter
        QString("$[?(@.%1 == 'test')]").arg(basePath),        // Equality filter
        QString("$[?(@.%1 > 0)]").arg(basePath),              // Comparison filter
        QString("$[?(@.%1 && @.other)]").arg(basePath),       // Logical AND filter
        QString("$[?(@.%1 || @.other)]").arg(basePath),       // Logical OR filter
        QString("$[?(!@.%1)]").arg(basePath),                 // Negation filter
        QString("$..*[?(@.%1)]").arg(basePath),               // Recursive filter
        QString("$[?(@.%1 =~ /pattern/)]").arg(basePath),     // Regex filter
        QString("$[?(@.%1 in ['a','b','c'])]").arg(basePath), // In filter
        QString("$[?(@.%1.length > 0)]").arg(basePath)        // Length filter
    };

    // Test each filter expression
    for (const QString& expression : filterExpressions)
    {
        try
        {
            auto pathResult = JSONPath::create(expression);
            if (pathResult)
            {
                // Test evaluation which internally uses filter storage
                auto result = pathResult->evaluate(doc);
                static_cast<void>(result); // Suppress unused warning
            }
        }
        catch (...)
        {
            // Should not throw, but if it does, continue testing
        }
    }

    // Test complex nested filter expressions
    QStringList complexFilters = {QString("$[?(@.%1 && (@.nested.value > 10 || @.other == 'test'))]").arg(basePath),
                                  QString("$..*[?(@.%1 && @.children[*].active)]").arg(basePath),
                                  QString("$[?(@.%1[*] && @.%1.length > 0)]").arg(basePath),
                                  QString("$[?(@.%1 =~ /^test/ && @.id > 0)]").arg(basePath)};

    for (const QString& expression : complexFilters)
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

    // Test with array contexts to exercise different filter scenarios
    if (doc.isArray())
    {
        QStringList arrayFilters = {QString("$[?(@.%1)]").arg(basePath),
                                    QString("$[*][?(@.%1)]").arg(basePath),
                                    QString("$[0,1,2][?(@.%1)]").arg(basePath)};

        for (const QString& expression : arrayFilters)
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
