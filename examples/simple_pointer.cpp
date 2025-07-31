// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QString>
#include <expected>

// Enable string literal operators for QString
using namespace Qt::StringLiterals;

#include "json-query/JSONQuery"

using json_query::JSONPointer;
using json_query::QueryError;
using json_query::toQString;
using json_query::toQStringView;

// Helper function to evaluate a JSON Pointer and print the result
void evaluateAndPrint(const JSONPointer& pointer, const QJsonDocument& doc, QStringView description)
{
    qDebug().noquote() << "\n" << description << ":";
    qDebug() << "  Pointer:" << (pointer.to_string().isEmpty() ? "\"\" (empty string)" : pointer.to_string());

    // Evaluate the pointer against the document
    auto result = pointer.evaluate(doc);

    if (!result)
    {
        qDebug() << "  Error:" << toQStringView(result.error());
        return;
    }

    // Special case: Show the whole document for empty pointer
    if (pointer.to_string().isEmpty())
    {
        qDebug() << "  Result: (whole document)";
        qDebug().noquote() << "  " << QJsonDocument(result->toObject()).toJson(QJsonDocument::Indented);
    }
    else
    {
        // Convert the result to a string for display
        QString resultStr;
        if (result->isObject() || result->isArray())
            resultStr = QJsonDocument(result->toObject()).toJson(QJsonDocument::Compact);
        else
            resultStr = result->toVariant().toString();

        qDebug() << "  Result:" << resultStr;
    }
}

// Helper function to create and evaluate a pointer
bool evaluatePointer(QStringView path, const QJsonDocument& doc, QStringView description)
{
    // Create the JSON Pointer
    auto pointer = JSONPointer::create(path);
    if (!pointer)
    {
        qWarning() << "Failed to create pointer:" << path << "-" << description;
        qWarning() << "  Error:" << toQStringView(pointer.error());
        return false;
    }

    evaluateAndPrint(*pointer, doc, description);
    return true;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // Sample JSON document demonstrating various JSON Pointer features
    QJsonObject docObj{
        {"name", "John Doe"},
        {"age", 42},
        {"active", true},
        {"address",
         QJsonObject{{"street", "123 Main St"}, {"city", "Anytown"}, {"coordinates", QJsonArray{12.34, 56.78}}}},
        {"tags", QJsonArray{"dev", "qt", "c++"}},
        {"special/chars", "value"}, // Key with a forward slash
        {"escaped~chars", "value"}, // Key with a tilde
        {"nested/array",
         QJsonArray{QJsonObject{{"id", 1}, {"value", "first"}}, QJsonObject{{"id", 2}, {"value", "second"}}}}};

    QJsonDocument doc(docObj);

    qDebug() << "=== JSON Document ===";
    qDebug().noquote() << doc.toJson(QJsonDocument::Indented);
    qDebug() << "====================";

    // 1. Basic access
    evaluatePointer(u"", doc, u"1. Empty pointer references the entire document");
    evaluatePointer(u"/name", doc, u"2. Simple property access");
    evaluatePointer(u"/age", doc, u"3. Numeric value access");
    evaluatePointer(u"/active", doc, u"4. Boolean value access");

    // 2. Nested object access
    evaluatePointer(u"/address/street", doc, u"5. Nested object property");
    evaluatePointer(u"/address/coordinates/0", doc, u"6. Nested array access (first element)");

    // 3. Array access
    evaluatePointer(u"/tags/0", doc, u"7. First array element (zero-based)");
    evaluatePointer(u"/tags/1", doc, u"8. Second array element");

    // 4. Special characters in keys
    evaluatePointer(u"/special~1chars", doc, u"9. Key with '/' (escaped as '~1')");
    evaluatePointer(u"/escaped~0chars", doc, u"10. Key with '~' (escaped as '~0')");

    // 5. Complex nested access
    evaluatePointer(u"/nested~1array/1/value", doc, u"11. Nested array with special characters in key");

    // 6. Error cases
    evaluatePointer(u"/nonexistent", doc, u"12. Non-existent path");
    evaluatePointer(u"/tags/10", doc, u"13. Array index out of bounds");
    evaluatePointer(u"/address/coordinates/not_an_index", doc, u"14. Invalid array index");

    // 7. Invalid pointer syntax
    qDebug() << "\n=== Error Cases ===";
    evaluatePointer(u"no_leading_slash", doc, u"15. Missing leading slash (invalid)");
    evaluatePointer(u"/invalid~escape", doc, u"16. Invalid escape sequence (invalid)");

    return EXIT_SUCCESS;
}
