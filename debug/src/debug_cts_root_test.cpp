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

    // Exact CTS test case: root selector on array document
    QJsonArray    testArray = QJsonArray{"first", "second"};
    QJsonDocument doc(testArray);

    qDebug() << "=== CTS ROOT TEST EXACT REPLICATION ===";
    qDebug() << "Test document:" << doc.toJson(QJsonDocument::Compact);

    // Compile selector (exactly like CTS)
    auto maybePath{JSONPath::create(QStringLiteral("$"))};
    if (!maybePath.has_value())
    {
        qDebug() << "Failed to compile:" << static_cast<int>(maybePath.error());
        return 1;
    }

    const auto& path = *maybePath;
    auto        result{path.evaluate(doc)}; // This is what CTS calls
    if (!result.has_value())
    {
        qDebug() << "Failed to evaluate:" << static_cast<int>(result.error());
        return 1;
    }

    QJsonValue resVal = *result;
    qDebug() << "Single evaluate() result type:" << resVal.type();
    qDebug() << "Single evaluate() result:" << QJsonDocument(QJsonArray{resVal}).toJson(QJsonDocument::Compact);

    // CTS test logic: manually convert to array (this is the problematic part)
    QJsonArray actual;
    if (resVal.isArray())
    {
        actual = resVal.toArray(); // This expands the array!
        qDebug() << "CTS logic: result is array, expanding to individual elements";
    }
    else
    {
        actual = QJsonArray{resVal};
        qDebug() << "CTS logic: result is not array, wrapping in array";
    }

    qDebug() << "CTS actual result count:" << actual.size();
    for (int i = 0; i < actual.size(); ++i)
        qDebug() << "CTS actual[" << i << "]:" << QJsonDocument(QJsonArray{actual[i]}).toJson(QJsonDocument::Compact);

    // Expected CTS result
    QJsonArray expected = QJsonArray{QJsonArray{"first", "second"}};
    qDebug() << "Expected CTS result count:" << expected.size();
    for (int i = 0; i < expected.size(); ++i)
        qDebug() << "Expected[" << i << "]:" << QJsonDocument(QJsonArray{expected[i]}).toJson(QJsonDocument::Compact);

    qDebug() << "Match:" << (actual == expected);

    return 0;
}
