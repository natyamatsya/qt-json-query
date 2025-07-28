// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// refactor_potential.cpp – demonstrates how json-query simplifies QJson handling
// Compares plain Qt JSON APIs with JSONPath-based approach provided by the library.
// Build target: refactor_potential (added in CMakeLists.txt)

#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QStringView>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include <type_traits>

#include "../include/json-query/json-pointer/JSONPointer.hpp"
#include "../include/json-query/json-path/JSONPath.hpp"
#include "../include/json-query/utils/JSONValueUtils.hpp"

using json_query::JSONPath;
using json_query::JSONPointer;
using json_query::utils::as;
using json_query::utils::errorMessage;

using namespace Qt::StringLiterals;

// -----------------------------------------------------------------------------
// Sample data – book store with inventory array
// -----------------------------------------------------------------------------
static QJsonDocument loadTestDocument()
{
    QJsonArray books;
    for (int i = 0; i < 10; ++i)
    {
        QJsonObject author{{"name", QString("Author %1").arg(i)}, {"born", 1950 + i}};
        QJsonObject edition{{"isbn", QString("978-1-%1").arg(1000 + i)}};
        QJsonObject details{{"author", author}, {"edition", edition}};
        books.append(QJsonObject{{"title", QString("Book %1").arg(i)}, {"price", 10 + i * 5}, {"details", details}});
    }

    QJsonObject store{{"inventory", books}};
    return QJsonDocument(store);
}

// -----------------------------------------------------------------------------
// 1) Plain Qt implementation – verbose manual traversal
// -----------------------------------------------------------------------------
static QStringList titlesAbovePrice_plain(const QJsonDocument& doc, double threshold)
{
    QStringList result;

    const auto root{doc.object()};
    const auto inventory{root.value("inventory").toArray()};

    for (const QJsonValue& v : inventory)
    {
        const auto obj{v.toObject()};
        const auto price{obj.value("price").toDouble()};
        if (price > threshold)
            result << obj.value("title").toString();
    }
    return result;
}

// -----------------------------------------------------------------------------
// Deeply nested access example
// Goal: retrieve the ISBN of book at given index (default 7)
// -----------------------------------------------------------------------------
static QString editionIsbn_plain(const QJsonDocument& doc, int index)
{
    const auto root{doc.object()};
    const auto inventory{root.value("inventory").toArray()};
    if (index < 0 || index >= inventory.size())
        return {};
    const auto book{inventory.at(index).toObject()};
    const auto details{book.value("details").toObject()};
    const auto edition{details.value("edition").toObject()};
    return edition.value("isbn").toString();
}

[[nodiscard]] static std::optional<QString> editionIsbn_pointer(const QJsonDocument& doc, int index)
{
    return JSONPointer::create(QString("/inventory/%1/details/edition/isbn").arg(index))
        .and_then([&doc](const JSONPointer& ptr) { return ptr.evaluate(doc); })
        .and_then([](const QJsonValue& v) { return as<QString>(v); })
        .value_or(std::nullopt);
}

[[nodiscard]] static std::optional<QString> editionIsbn_path(const QJsonDocument& doc, int index)
{
    return JSONPath::create(QString(u"$.inventory[%1].details.edition.isbn").arg(index))
        .and_then([&doc](const JSONPath& path) { return path.evaluate(doc); })
        .and_then(
            [](const QJsonValue& v)
            {
                if (v.isArray())
                {
                    const auto arr = v.toArray();
                    return arr.isEmpty() ? std::optional<QJsonValue>{} : std::optional{arr.first()};
                }
                return std::optional{v};
            })
        .and_then([](const QJsonValue& v) { return as<QString>(v); })
        .value_or(std::nullopt);
}

// -----------------------------------------------------------------------------
// 2) Library implementation using JSONPath – concise & expressive
// -----------------------------------------------------------------------------
[[nodiscard]] static QStringList titlesAbovePrice_jsonquery(const QJsonDocument& doc, double threshold)
{
    return JSONPath::create(QString("$.inventory[?(@.price > %1)].title").arg(threshold))
        .and_then([&doc](const JSONPath& path) { return path.evaluate(doc); })
        .transform(
            [](const QJsonValue& v)
            {
                QStringList result;
                if (v.isArray())
                    for (const auto& item : v.toArray())
                        as<QString>(item).transform([&](const QString& s) { result << s; });
                else if (!v.isUndefined())
                    as<QString>(v).transform([&](const QString& s) { result << s; });
                return result;
            })
        .value_or(QStringList{});
}

int main(int argc, char** argv)
{
    QCoreApplication    app(argc, argv); // Needed for QString conversions on some platforms
    const QJsonDocument doc       = loadTestDocument();
    constexpr double    threshold = 25.0;

    // Test title queries
    const auto plainTitles = titlesAbovePrice_plain(doc, threshold);
    const auto queryTitles = titlesAbovePrice_jsonquery(doc, threshold);

    qDebug() << "=== Books with price >" << threshold << "===";
    qDebug() << "Plain QtJSON   :" << plainTitles;
    qDebug() << "json-query path:" << queryTitles;

    // Test ISBN retrieval for different books
    const std::array<int, 3> testIndices{0, 5, 7};
    qDebug() << "\n=== ISBN Retrieval ===";

    for (const int index : testIndices)
    {
        const auto plainIsbn   = editionIsbn_plain(doc, index);
        const auto pointerIsbn = editionIsbn_pointer(doc, index).value_or("(not found)");
        const auto pathIsbn    = editionIsbn_path(doc, index).value_or("(not found)");

        qDebug() << "\nBook index:" << index;
        qDebug() << "  Plain:   " << plainIsbn;
        qDebug() << "  Pointer: " << pointerIsbn;
        qDebug() << "  Path:    " << pathIsbn;
    }

    // Test error case (invalid index)
    const int invalidIndex = 100;
    qDebug() << "\n=== Error Case (invalid index:" << invalidIndex << ") ===";
    const auto invalidPointer = editionIsbn_pointer(doc, invalidIndex);
    const auto invalidPath    = editionIsbn_path(doc, invalidIndex);

    qDebug() << "Pointer result:" << (invalidPointer ? *invalidPointer : "(not found)");
    qDebug() << "Path result:   " << (invalidPath ? *invalidPath : "(not found)");

    return 0;
}
