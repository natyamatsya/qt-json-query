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

    // ─── helper: always give me an array ───────────────────────────────────────
    auto toArray = [](QJsonValue v) -> QJsonArray {
        if (v.isArray())            return v.toArray();
        if (!v.isUndefined())       return QJsonArray{ v };
        return {};
    };

    // ─── one monadic pipeline ─────────────────────────────────────────────────
    JSONPath::create(u"$.books[?(@.price > 20)].author")          // Result<JSONPath>
        .transform([&](const JSONPath& jp) {                      // -> QJsonValue
            return jp.evaluate(doc);
        })
        .transform(toArray)                                       // -> QJsonArray
        .and_then([](const QJsonArray& authors) {                 // happy path
            qInfo() << "Authors of expensive books:" << authors;
            return std::expected<void, json_query::Error>{};      // propagate Ok
        })
        .or_else([](json_query::Error e) -> std::expected<void,json_query::Error>
        {
            // error path
            qWarning() << "Invalid JSONPath:" << json_query::toString(e).data();
            return {};
        });

    return EXIT_SUCCESS;
}
