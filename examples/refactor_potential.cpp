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

#include "../include/json-query/JSONPointer.hpp"
#include "../include/json-query/json-path/JSONPath.hpp"

using json_query::JSONPointer;
using json_query::JSONPath;

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
        books.append(QJsonObject{{"title", QString("Book %1").arg(i)},
                                 {"price", 10 + i * 5},
                                 {"details", details}});
    }

    QJsonObject store{{"inventory", books}};
    return QJsonDocument(store);
}

// -----------------------------------------------------------------------------
// 1) Plain Qt implementation – verbose manual traversal
// -----------------------------------------------------------------------------
static QStringList titlesAbovePrice_plain(const QJsonDocument &doc, double threshold)
{
    QStringList result;

    const QJsonObject root = doc.object();
    const QJsonArray inventory = root.value("inventory").toArray();

    for (const QJsonValue &v : inventory)
    {
        const QJsonObject obj = v.toObject();
        const double price = obj.value("price").toDouble();
        if (price > threshold)
            result << obj.value("title").toString();
    }
    return result;
}

// -----------------------------------------------------------------------------
// Deeply nested access example
// Goal: retrieve the ISBN of book at given index (default 7)
// -----------------------------------------------------------------------------
static QString editionIsbn_plain(const QJsonDocument &doc, int index)
{
    const QJsonObject root = doc.object();
    const QJsonArray inventory = root.value("inventory").toArray();
    if (index < 0 || index >= inventory.size())
        return {};
    const QJsonObject book = inventory.at(index).toObject();
    const QJsonObject details = book.value("details").toObject();
    const QJsonObject edition = details.value("edition").toObject();
    return edition.value("isbn").toString();
}

static QString editionIsbn_pointer(const QJsonDocument &doc, int index)
{
    auto ptr = JSONPointer::create(QString("/inventory/%1/details/edition/isbn").arg(index));
    if (!ptr)
        return {};
    return ptr->evaluate(doc).toString();
}

static QString editionIsbn_path(const QJsonDocument &doc, int index)
{
    auto pathResult{JSONPath::create(QString(u"$.inventory[%1].details.edition.isbn").arg(index))};
    QJsonValue r{ pathResult->evaluate(doc) };
    if (r.isArray()) {
        const auto arr = r.toArray();
        return arr.isEmpty() ? QString() : arr.first().toString();
    }
    return r.toString();
}

// -----------------------------------------------------------------------------
// 2) Library implementation using JSONPath – concise & expressive
// -----------------------------------------------------------------------------
static QStringList titlesAbovePrice_jsonquery(const QJsonDocument &doc, double threshold)
{
    // Build a JSONPath expression with the price threshold.
    QStringView pathView = QString("$.inventory[?(@.price > %1)].title").arg(threshold);
    auto query{ JSONPath::create(pathView) };
    QJsonValue r = query->evaluate(doc);
    QStringList result;
    if (r.isArray()) {
        for (const QJsonValue &v : r.toArray())
            result << v.toString();
    } else if (!r.isUndefined()) {
        result << r.toString();
    }
    return result;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv); // Needed for QString conversions on some platforms

    const QJsonDocument doc = loadTestDocument();
    constexpr double threshold = 25.0;

    const QStringList plain = titlesAbovePrice_plain(doc, threshold);
    const QStringList viaQuery = titlesAbovePrice_jsonquery(doc, threshold);

    qDebug() << "Plain QtJSON   :" << plain;
    qDebug() << "json-query path:" << viaQuery;

        // Deep nested ISBN retrieval demo
    const int bookIndex = 7;
    qDebug() << "ISBN plain      :" << editionIsbn_plain(doc, bookIndex);
    qDebug() << "ISBN via pointer:" << editionIsbn_pointer(doc, bookIndex);
    qDebug() << "ISBN via path   :" << editionIsbn_path(doc, bookIndex);

    return 0;
}
