#include "json-query/JSONQuery"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace json_query;

// Import the as<T> function and error utilities
using json_query::as;
// QueryError::message_qt() provides human-readable QStringView

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
        qDebug() << "1. Error getting store name:" << storeName.error().message_qt();

    // 1b. Same using the PIPE syntax
    auto storeNamePipe = (doc["store"]["name"] | as<QString>);
    if (storeNamePipe)
        qDebug() << "1b. Store name (pipe):" << *storeNamePipe;
    else
        qDebug() << "1b. Error getting store name (pipe):" << storeNamePipe.error().message_qt();

    // 2. Get the first book
    auto firstBook = as<QJsonObject>(doc["store"]["book"][0]);
    if (firstBook)
    {
        // 2.1 Get book title as string
        auto title = as<QString>((*firstBook)["title"]);
        if (title)
            qDebug() << "2.1 Book title:" << *title;
        else
            qDebug() << "2.1 Error getting title:" << title.error().message_qt();

        // 2.1b Title using PIPE syntax
        auto titlePipe = (((*firstBook)["title"]) | as<QString>);
        if (titlePipe)
            qDebug() << "2.1b Book title (pipe):" << *titlePipe;
        else
            qDebug() << "2.1b Error getting title (pipe):" << titlePipe.error().message_qt();

        // 2.2 Get book price as double
        auto price = as<double>((*firstBook)["price"]);
        if (price)
            qDebug() << "2.2 Book price:" << *price;
        else
            qDebug() << "2.2 Error getting price:" << price.error().message_qt();

        // 2.2b Price using PIPE syntax
        auto pricePipe = (((*firstBook)["price"]) | as<double>);
        if (pricePipe)
            qDebug() << "2.2b Book price (pipe):" << *pricePipe;
        else
            qDebug() << "2.2b Error getting price (pipe):" << pricePipe.error().message_qt();
    }
    else
    {
        qDebug() << "2. Error getting first book:" << firstBook.error().message_qt();
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
                qWarning() << "3. Error getting first inventory item:" << firstItem.error().message_qt();
        }
        else
        {
            qDebug() << "3. No inventory items found";
        }
    }
    else
    {
        qDebug() << "3. Error getting store:" << store.error().message_qt();
    }

    // 4. Pipe example for a missing key (to show error propagation)
    auto missingPipe = (doc["store"]["doesNotExist"] | as<QString>);
    if (!missingPipe)
        qDebug() << "4. Pipe error example:" << missingPipe.error().message_qt();

    return 0;
}
