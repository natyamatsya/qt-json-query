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

    JSONPath::create(u"$.books[*].title")                 // expected<JSONPath,Error>
        .transform([&](const JSONPath& jp) {              // → QJsonValue
            return jp.evaluate(doc);
        })
        .transform(toArray)                               // → QJsonArray
        .and_then([](const QJsonArray& titles) {          // success branch
            qInfo() << "Book titles:" << titles;          // ["Book 1", "Book 2", "Book 3"]
            return std::expected<void, Error>{};
        })
        .or_else([](Error e) -> std::expected<void, Error>
        {                // error branch
            qWarning() << "Invalid JSONPath:" << toString(e).data();
            return {};
        });

    return EXIT_SUCCESS;
}
