// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/JSONQuery"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

int main(int argc, char** argv)
{
    using namespace json_query;
    using namespace json_query::json_path;

    QCoreApplication app{argc, argv};

    const auto doc{QJsonDocument{QJsonObject{
        {"books",
         QJsonArray{
             QJsonObject{{"title", "Book 1"}, {"price", 10}},
             QJsonObject{{"title", "Book 2"}, {"price", 20}},
             QJsonObject{{"title", "Book 3"}, {"price", 30}},
         }},
    }}};

    const auto path{JSONPath::create(u"$.books[*].title")};
    if (!path)
    {
        qWarning() << "Failed to compile JSONPath:" << path.error().message_qt();
        return EXIT_FAILURE;
    }

    const auto results{path->evaluate(doc)};
    if (!results)
    {
        qWarning() << "Failed to evaluate JSONPath:" << results.error().message_qt();
        return EXIT_FAILURE;
    }

    qDebug() << "Book titles:";
    for (const auto& title : *results)
        qDebug() << "  -" << title.toString();

    return EXIT_SUCCESS;
}
