#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>

#include "../include/json-query/json-pointer/JSONPointer.hpp"

using json_query::JSONPointer;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv); // Needed for some Qt setups (not strictly necessary here but harmless)

    // Create a simple JSON document { "foo": 42 }
    QJsonObject obj{{"foo", 42}};
    QJsonDocument doc(obj);

    // Create JSON Pointer via factory
    auto pointer = JSONPointer::create(QStringLiteral("/foo"));
    if (!pointer) {
        qWarning() << "Invalid JSON Pointer.";
        return -1;
    }

    auto res = pointer->evaluate(doc);
    qDebug() << (res ? res->toString() : QString("<err>"));
    return EXIT_SUCCESS;
}
