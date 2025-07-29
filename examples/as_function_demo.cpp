#include "json-query/utils/JSONValueUtils.hpp"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// Import the as<T> function and error utilities
using json_query::as;
using json_query::errorMessage;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // Example JSON data
    const char* jsonData = R"(
    {
        "store": {
            "name": "The Book Emporium",
            "book": [
                {
                    "title": "The Great Gatsby",
                    "author": "F. Scott Fitzgerald",
                    "price": 12.99,
                    "inStock": true
                },
                {
                    "title": "To Kill a Mockingbird",
                    "author": "Harper Lee",
                    "price": 10.50,
                    "inStock": false
                }
            ],
            "inventory": [
                {
                    "id": 1,
                    "inStock": true,
                    "quantity": 5
                }
            ]
        }
    }
    )";

    // Parse the JSON document
    QJsonParseError error;
    QJsonDocument   doc = QJsonDocument::fromJson(jsonData, &error);
    if (doc.isNull())
    {
        qWarning() << "Failed to parse JSON:" << error.errorString();
        return 1;
    }

    qDebug() << "=== Demonstrating as<T> Function ===\n";

    // 1. Get store name as string
    auto storeName = as<QString>(doc["store"]["name"]);
    if (storeName)
        qDebug() << "1. Store name:" << *storeName;
    else
        qDebug() << "1. Error getting store name:" << errorMessage(storeName.error());

    // 2. Get the first book
    auto firstBook = as<QJsonObject>(doc["store"]["book"][0]);
    if (firstBook)
    {
        // 2.1 Get book title as string
        auto title = as<QString>((*firstBook)["title"]);
        if (title)
            qDebug() << "2.1 Book title:" << *title;
        else
            qDebug() << "2.1 Error getting title:" << errorMessage(title.error());

        // 2.2 Get book price as double
        auto price = as<double>((*firstBook)["price"]);
        if (price)
            qDebug() << "2.2 Book price:" << *price;
        else
            qDebug() << "2.2 Error getting price:" << errorMessage(price.error());
    }
    else
    {
        qDebug() << "2. Error getting first book:" << errorMessage(firstBook.error());
    }

    // 3. Check inventory status
    auto store = as<QJsonObject>(doc["store"]);
    if (store)
    {
        auto inventory = as<QJsonArray>((*store)["inventory"]);
        if (inventory && !inventory->isEmpty())
        {
            auto firstItem = as<QJsonObject>((*inventory)[0]);
            if (firstItem)
                qDebug() << "3. First item in stock:" << (*firstItem)["inStock"].toBool();
            else
                qDebug() << "3. Error getting first inventory item";
        }
        else
        {
            qDebug() << "3. No inventory items found";
        }
    }
    else
    {
        qDebug() << "3. Error getting store:" << errorMessage(store.error());
    }

    return 0;
}
