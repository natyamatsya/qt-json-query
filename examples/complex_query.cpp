#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "json-query/JSONPath.hpp"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    // Simulate a store with books and authors
    QJsonArray books{
        QJsonObject{{"title", "Book 1"}, {"author", "Author 1"}, {"price", 10}},
        QJsonObject{{"title", "Book 2"}, {"author", "Author 2"}, {"price", 25}},
        QJsonObject{{"title", "Book 3"}, {"author", "Author 1"}, {"price", 15}},
        QJsonObject{{"title", "Book 4"}, {"author", "Author 3"}, {"price", 30}}
    };
    QJsonObject store{{"books", books}};
    QJsonDocument doc(store);

    // Query: authors of books costing more than 20
    JSONPath path("$.books[?(@.price > 20)].author");
    if (!path.isValid()) {
        qWarning() << "Invalid JSONPath.";
        return EXIT_FAILURE;
    }

    QJsonValue res = path.evaluate(doc);
    QJsonArray result;
    if (res.isArray())
        result = res.toArray();
    else if (!res.isUndefined())
        result.append(res);
    qInfo() << "Authors of expensive books:" << result; // Expected ["Author 2", "Author 3"]

    return EXIT_SUCCESS;
}
