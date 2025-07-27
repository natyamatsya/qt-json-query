#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDebug>
#include "json-query/json-path/JSONPath.hpp"

using json_query::JSONPath;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Test case from CTS: root selector on array document
    QJsonArray    testDoc = QJsonArray{"first", "second"};
    QJsonDocument doc(testDoc);

    qDebug() << "=== ROOT ARRAY TEST DEBUG ===";
    qDebug() << "Test document:" << doc.toJson(QJsonDocument::Compact);

    // Test root selector
    auto jsonPath{JSONPath::create(QStringLiteral("$"))};
    if (!jsonPath)
    {
        qDebug() << "Failed to compile JSONPath:" << static_cast<int>(jsonPath.error());
        return 1;
    }

    // Test evaluate (single result)
    auto singleResult{jsonPath->evaluate(testDoc)};
    if (!singleResult)
    {
        qDebug() << "Failed to evaluate:" << static_cast<int>(singleResult.error());
        return 1;
    }

    qDebug() << "Single result type:" << singleResult->type();
    qDebug() << "Single result:" << QJsonDocument(QJsonArray{*singleResult}).toJson(QJsonDocument::Compact);

    // Test evaluateAll (multiple results)
    auto allResults{jsonPath->evaluateAll(testDoc)};
    if (!allResults)
    {
        qDebug() << "Failed to evaluateAll:" << static_cast<int>(allResults.error());
        return 1;
    }

    qDebug() << "All results count:" << allResults->size();
    for (int i = 0; i < allResults->size(); ++i)
    {
        qDebug() << "Result[" << i
                 << "]:" << QJsonDocument(QJsonArray{allResults->at(i)}).toJson(QJsonDocument::Compact);
    }

    qDebug() << "Expected CTS result: [[\"first\",\"second\"]]";
    qDebug() << "Actual result count:" << allResults->size() << "(should be 1)";

    return 0;
}
