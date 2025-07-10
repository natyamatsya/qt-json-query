#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "json-query/JSONPath.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // Create a simple JSON document with an array of objects
    QJsonArray books{
        QJsonObject{{"title", "Book 1"}, {"price", 10}},
        QJsonObject{{"title", "Book 2"}, {"price", 20}},
        QJsonObject{{"title", "Book 3"}, {"price", 30}}
    };
    QJsonObject store{{"books", books}};
    QJsonDocument doc(store);

    // Query all titles
    JSONPath path("$.books[*].title");
    if (!path.isValid()) {
        qWarning() << "Invalid JSONPath.";
        return EXIT_FAILURE;
    }

    QJsonArray titles = path.evaluate(doc);
    qInfo() << "Book titles:" << titles; // Expected: ["Book 1", "Book 2", "Book 3"]

    return EXIT_SUCCESS;
}
