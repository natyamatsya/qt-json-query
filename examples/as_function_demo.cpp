// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/JSONQuery"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using json_query::as;

int main(int argc, char* argv[])
{
    QCoreApplication app{argc, argv};

    const auto doc{QJsonDocument{QJsonObject{
        {"store",
         QJsonObject{
             {"name", "The Book Emporium"},
             {"book",
              QJsonArray{
                  QJsonObject{{"title", "The Great Gatsby"}, {"author", "F. Scott Fitzgerald"}, {"price", 12.99}, {"inStock", true}},
                  QJsonObject{{"title", "To Kill a Mockingbird"}, {"author", "Harper Lee"}, {"price", 10.50}, {"inStock", false}},
              }},
             {"inventory",
              QJsonArray{
                  QJsonObject{{"id", 1}, {"inStock", true}, {"quantity", 5}},
              }},
         }},
    }}};

    const auto root{doc.object()};

    qDebug() << "=== as<T> Type-Safe Conversions ===";

    // ── 1. Call syntax: as<T>(value) ────────────────────────────────
    qDebug() << "\n-- Call syntax --";

    const auto storeName{as<QString>(root["store"]["name"])};
    qDebug() << "Store name:" << storeName.value_or("(error)");

    const auto price{as<double>(root["store"]["book"][0]["price"])};
    if (price)
        qDebug() << "First book price:" << *price;

    const auto inStock{as<bool>(root["store"]["book"][0]["inStock"])};
    if (inStock)
        qDebug() << "First book in stock:" << *inStock;

    // ── 2. Pipe syntax: value | as<T> ──────────────────────────────
    qDebug() << "\n-- Pipe syntax --";

    const auto title{root["store"]["book"][0]["title"] | as<QString>};
    qDebug() << "First book title:" << title.value_or("(error)");

    const auto author{root["store"]["book"][1]["author"] | as<QString>};
    qDebug() << "Second book author:" << author.value_or("(error)");

    const auto quantity{root["store"]["inventory"][0]["quantity"] | as<int>};
    if (quantity)
        qDebug() << "Inventory quantity:" << *quantity;

    // ── 3. Error handling ──────────────────────────────────────────
    qDebug() << "\n-- Error cases --";

    // Missing key
    const auto missing{root["store"]["doesNotExist"] | as<QString>};
    if (!missing)
        qDebug() << "Missing key:" << missing.error().message_qt();

    // Type mismatch: name is a string, not an int
    const auto wrongType{as<int>(root["store"]["name"])};
    if (!wrongType)
        qDebug() << "Wrong type:" << wrongType.error().message_qt();

    return 0;
}
