// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QString>
#include <QStringConverter>
#include <expected>

// Enable string literal operators for QString
using namespace Qt::StringLiterals;

#include "json-query/JSONQuery"

using json_query::JSONPointer;
using json_query::json_pointer::ParseError;

void evaluateAndPrint(const JSONPointer& pointer, const QJsonDocument& doc, QStringView desc)
{
    qDebug().noquote() << "\n" << desc << ":";
    qDebug() << "  Pointer:" << (pointer.toString().isEmpty() ? "\"\" (empty string)" : pointer.toString());

    auto result = pointer.evaluate(doc);
    if (!result)
    {
        qDebug() << "  Error:   Could not evaluate pointer";
        return;
    }

    if (pointer.to_string().isEmpty())
    {
        // Special case: Show the whole document for empty pointer
        qDebug() << "  Result:  (whole document)";
        qDebug().noquote() << "  " << QJsonDocument(result->toObject()).toJson(QJsonDocument::Indented);
        return;
    }

    qDebug() << "  Result:  " << result->toVariant().toString();
}

// Helper function to create a pointer and handle errors
bool evaluatePointer(QStringView path, const QJsonDocument& doc, QStringView description)
{
    auto pointer = JSONPointer::create(path);
    if (!pointer)
    {
        qWarning() << "Failed to create pointer:" << path << "-" << description;
        return false;
    }

    evaluateAndPrint(*pointer, doc, description);
    return true;
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);

    // Sample JSON document
    QJsonObject   docObj{{"name", "John Doe"},
                         {"age", 42},
                         {"active", true},
                         {"address", QJsonObject{{"street", "123 Main St"}, {"city", "Anytown"}}},
                         {"tags", QJsonArray{"dev", "qt", "c++"}},
                         {"special/chars", "value"},
                         {"escaped~chars", "value"}};
    QJsonDocument doc(docObj);

    qDebug() << "=== JSON Document ===";
    qDebug().noquote() << doc.toJson(QJsonDocument::Indented);
    qDebug() << "====================";

    // RFC 6901 Section 5: Syntax
    // The empty string evaluates to the whole document
    evaluatePointer(u"", doc, u"RFC 6901 Section 5: Empty pointer references the entire document");

    // RFC 6901 Section 3: JSON Pointer Syntax
    // Simple property access using a single reference token
    evaluatePointer(u"/name", doc, u"RFC 6901 Section 3: Simple property access");

    // RFC 6901 Section 3: JSON Pointer Syntax
    // Nested object access using multiple reference tokens
    evaluatePointer(u"/address/city", doc, u"RFC 6901 Section 3: Nested object access");

    // RFC 6901 Section 4: Evaluation
    // Array access using a zero-based index
    evaluatePointer(u"/tags/1", doc, u"RFC 6901 Section 4: Array access (zero-based index)");

    // RFC 6901 Section 3: JSON Pointer Syntax
    // Special character handling: '~1' is the escape sequence for '/'
    evaluatePointer(u"/special~1chars", doc, u"RFC 6901 Section 3: Special character '/' in key (escaped as '~1')");

    // RFC 6901 Section 3: JSON Pointer Syntax
    // Tilde escaping: '~0' is the escape sequence for '~'
    evaluatePointer(u"/escaped~0chars", doc, u"RFC 6901 Section 3: Tilde in key (escaped as '~0')");

    // RFC 6901 Section 5: Evaluation
    // Non-existent path evaluation
    evaluatePointer(u"/nonexistent", doc, u"RFC 6901 Section 5: Non-existent path evaluation");

    // RFC 6901 Section 3: JSON Pointer Syntax
    // Example of an invalid pointer (doesn't start with '/' or be empty)
    auto invalidPointer = JSONPointer::create(u"invalid");
    if (!invalidPointer)
    {
        qDebug() << "\nRFC 6901 Section 3: Invalid pointer (must be empty or start with '/'):";
        qDebug() << "  Failed to create pointer: invalid";
    }

    return EXIT_SUCCESS;
}
