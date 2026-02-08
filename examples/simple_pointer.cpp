// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/JSONQuery"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using json_query::JSONPointer;

// Create a pointer, evaluate it, and print the result
static void evaluatePointer(QStringView path, const QJsonValue& doc, QStringView description)
{
    qDebug().noquote() << "\n" << description;

    const auto result{
        JSONPointer::create(path)
            .and_then([&](const JSONPointer& ptr) { return ptr.evaluate(doc); })};

    if (!result)
    {
        qDebug() << "  Error:" << result.error().message_qt();
        return;
    }

    if (result->isObject() || result->isArray())
        qDebug().noquote() << "  =" << QJsonDocument{result->toObject()}.toJson(QJsonDocument::Compact);
    else
        qDebug() << "  =" << result->toVariant().toString();
}

int main(int argc, char** argv)
{
    QCoreApplication app{argc, argv};

    const auto doc{QJsonValue{QJsonObject{
        {"name", "John Doe"},
        {"age", 42},
        {"active", true},
        {"address",
         QJsonObject{{"street", "123 Main St"}, {"city", "Anytown"}, {"coordinates", QJsonArray{12.34, 56.78}}}},
        {"tags", QJsonArray{"dev", "qt", "c++"}},
        {"special/chars", "value"},
        {"escaped~chars", "value"},
        {"nested/array",
         QJsonArray{QJsonObject{{"id", 1}, {"value", "first"}}, QJsonObject{{"id", 2}, {"value", "second"}}}},
    }}};

    qDebug() << "=== JSON Pointer (RFC 6901) ===";

    // Basic access
    evaluatePointer(u"", doc, u"Empty pointer → entire document");
    evaluatePointer(u"/name", doc, u"Simple property");
    evaluatePointer(u"/age", doc, u"Numeric value");
    evaluatePointer(u"/active", doc, u"Boolean value");

    // Nested + array access
    evaluatePointer(u"/address/street", doc, u"Nested property");
    evaluatePointer(u"/address/coordinates/0", doc, u"Nested array element");
    evaluatePointer(u"/tags/0", doc, u"Array element (zero-based)");
    evaluatePointer(u"/tags/1", doc, u"Second array element");

    // Escape sequences: '~0' = '~', '~1' = '/'
    evaluatePointer(u"/special~1chars", doc, u"Key containing '/' (escaped as ~1)");
    evaluatePointer(u"/escaped~0chars", doc, u"Key containing '~' (escaped as ~0)");
    evaluatePointer(u"/nested~1array/1/value", doc, u"Combined escaping + nested access");

    // Error cases
    qDebug() << "\n=== Error cases ===";
    evaluatePointer(u"/nonexistent", doc, u"Non-existent path");
    evaluatePointer(u"/tags/10", doc, u"Array index out of bounds");
    evaluatePointer(u"no_leading_slash", doc, u"Missing leading slash");
    evaluatePointer(u"/invalid~escape", doc, u"Invalid escape sequence");

    return EXIT_SUCCESS;
}
