#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "../include/json-query/json-path/JSONPath.hpp"

int main(int argc, char **argv)
{
    using namespace json_query;
    using namespace json_query::json_path;

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

    // ─── helper: always give me an array ───────────────────────────────────────
    auto toArray = [](QJsonValue v) -> QJsonArray {
        if (v.isArray())            return v.toArray();
        if (!v.isUndefined())       return QJsonArray{ v };
        return {};
    };

    auto pathResult = JSONPath::create(u"$.books[?(@.price > 20)].author");
    if (!pathResult) {
        qDebug() << "Failed to compile JSONPath";
        return EXIT_FAILURE;
    }
    
    auto evalResult = pathResult->evaluate(doc);
    if (!evalResult) {
        qDebug() << "Failed to evaluate JSONPath";
        return EXIT_FAILURE;
    }
    
    QJsonArray authors = toArray(*evalResult);
    qDebug() << "Authors of expensive books:";
    for (const auto& author : authors) {
        qDebug() << "  -" << author.toString();
    }

    return EXIT_SUCCESS;
}
