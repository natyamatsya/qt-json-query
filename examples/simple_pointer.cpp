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

    // Create and evaluate JSON Pointer
    auto pointerExp = JSONPointer::create("/foo");
    if (!pointerExp) {
        qWarning() << "Failed to parse JSON Pointer.";
        return EXIT_FAILURE;
    }

    auto resultExp = pointerExp->evaluate(doc);
    if (!resultExp) {
        qWarning() << "Failed to evaluate JSON Pointer.";
        return EXIT_FAILURE;
    }

    qInfo() << "Pointer result:" << resultExp.value(); // Should output: 42
    return EXIT_SUCCESS;
}
