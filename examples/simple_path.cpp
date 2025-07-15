#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>

#include "json-query/JSONPath.hpp"

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

    // Small utility: whatever comes back, give me a QJsonArray
    const auto toArray = [](QJsonValue v) -> QJsonArray {
        if (v.isArray())            return v.toArray();
        if (!v.isUndefined())       return QJsonArray{ v };
        return {};
    };

    JSONPath::create(u"$.books[*].title")                 // expected<JSONPath,Error>
        .transform([&](const JSONPath& jp) {              // → QJsonValue
            return jp.evaluate(doc);
        })
        .transform(toArray)                               // → QJsonArray
        .and_then([](const QJsonArray& titles) {          // success branch
            qInfo() << "Book titles:" << titles;          // ["Book 1", "Book 2", "Book 3"]
            return std::expected<void, json_query::Error>{};
        })
        .or_else([](json_query::Error e) -> std::expected<void,json_query::Error> {                // error branch
            qWarning() << "Invalid JSONPath:" << json_query::toString(e).data();
            return {};
        });

    return EXIT_SUCCESS;
}
