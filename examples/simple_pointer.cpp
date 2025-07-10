#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

#include "json-query/JSONPointer.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv); // Needed for some Qt setups (not strictly necessary here but harmless)

    // Create a simple JSON document { "foo": 42 }
    QJsonObject obj{{"foo", 42}};
    QJsonDocument doc(obj);

    // Create JSON Pointer directly
    JSONPointer pointer("/foo");
    if (!pointer.isValid()) {
        qWarning() << "Invalid JSON Pointer.";
        return EXIT_FAILURE;
    }

    QJsonValue result = pointer.evaluate(doc);
    if (result.isUndefined()) {
        qWarning() << "Failed to evaluate JSON Pointer.";
        return EXIT_FAILURE;
    }

    qInfo() << "Pointer result:" << result; // Should output: 42
    return EXIT_SUCCESS;
}
