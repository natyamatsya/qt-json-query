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

    // Create a simple JSON document with an array of objects
    QJsonArray books{
        QJsonObject{{"title", "Book 1"}, {"price", 10}},
        QJsonObject{{"title", "Book 2"}, {"price", 20}},
        QJsonObject{{"title", "Book 3"}, {"price", 30}}
    };
    QJsonObject store{{"books", books}};
    QJsonDocument doc(store);

    // Small utility: whatever comes back, give me a QJsonArray
    const auto toArray = [](QJsonValue v) -> QJsonArray {
        if (v.isArray())            return v.toArray();
        if (!v.isUndefined())       return QJsonArray{ v };
        return {};
    };

    auto pathResult = JSONPath::create(u"$.books[*].title");
    if (!pathResult) {
        qDebug() << "Failed to compile JSONPath";
        return EXIT_FAILURE;
    }
    
    auto evalResult = pathResult->evaluateExpected(doc);
    if (!evalResult) {
        qDebug() << "Failed to evaluate JSONPath";
        return EXIT_FAILURE;
    }
    
    QJsonArray titles = toArray(*evalResult);
    qDebug() << "Book titles:";
    for (const auto& title : titles) {
        qDebug() << "  -" << title.toString();
    }

    return EXIT_SUCCESS;
}
