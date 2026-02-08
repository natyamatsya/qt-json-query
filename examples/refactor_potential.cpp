// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// refactor_potential.cpp – demonstrates how json-query simplifies QJson handling
// Compares plain Qt JSON APIs with JSONPath-based approach provided by the library.
// Build target: refactor_potential (added in CMakeLists.txt)

#include "json-query/JSONQuery"

#include <expected>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRandomGenerator>
#include <QString>
#include <QStringList>
#include <QTextStream>

#include <type_traits>
#include <variant>

using json_query::as;
using json_query::Error;
using json_query::JSONPath;
using json_query::JSONPointer;

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
[[nodiscard]] static std::optional<QString> editionIsbn_plain(const QJsonDocument& doc, int index)
{
    // Check if root is an object
    if (!doc.isObject())
    {
        qWarning() << "Document root is not an object";
        return std::nullopt;
    }

    // Get inventory array
    const auto root = doc.object();
    if (!root.contains("inventory"))
    {
        qWarning() << "No inventory found in document";
        return std::nullopt;
    }

    const auto inventory = root.value("inventory").toArray();
    if (index < 0 || index >= inventory.size())
    {
        qWarning() << "Index out of range:" << index << "(size:" << inventory.size() << ")";
        return std::nullopt;
    }

    // Get book object
    const auto book = inventory.at(index).toObject();
    if (book.isEmpty())
    {
        qWarning() << "Book at index" << index << "is not a valid object";
        return std::nullopt;
    }

    // Get details object
    if (!book.contains("details"))
    {
        qWarning() << "No details found for book at index" << index;
        return std::nullopt;
    }

    const auto details = book.value("details").toObject();
    if (details.isEmpty())
    {
        qWarning() << "Details is not a valid object for book at index" << index;
        return std::nullopt;
    }

    // Get edition object
    if (!details.contains("edition"))
    {
        qWarning() << "No edition information found in details for book at index" << index;
        return std::nullopt;
    }

    const auto edition = details.value("edition").toObject();
    if (edition.isEmpty())
    {
        qWarning() << "Edition is not a valid object for book at index" << index;
        return std::nullopt;
    }

    // Get ISBN
    if (!edition.contains("isbn"))
    {
        qWarning() << "No ISBN field found in edition for book at index" << index;
        return std::nullopt;
    }

    const auto isbn = edition.value("isbn");
    if (!isbn.isString())
    {
        qWarning() << "ISBN is not a string for book at index" << index << "(type:" << isbn.type() << ")";
        return std::nullopt;
    }

    return isbn.toString();
}

// Helper function to log query errors

static auto log_query_error(const QString& context)
{
    return [&](const Error& error)
    {
        qWarning() << context << ":" << error.message_qt();
        return error;
    };
}

[[nodiscard]] static std::optional<QString> editionIsbn_pointer(const QJsonDocument& doc, int index)
{
    auto path        = QString("/inventory/%1/details/edition/isbn").arg(index);
    auto evaluate    = [&doc](const auto&& pointer) { return pointer.evaluate(doc); };
    auto to_optional = [](const auto&& s) -> std::optional<QString> { return s; };

    return JSONPointer::create(path)
        .transform_error(log_query_error(QString("Failed to create JSONPointer for path %1").arg(path)))
        .and_then(evaluate)
        .transform_error(log_query_error(QString("Failed to evaluate JSONPointer %1").arg(path)))
        .and_then(as<QString>)
        .transform_error(log_query_error(QString("Failed to convert result")))
        .transform(to_optional)
        .value_or(std::nullopt);
}

[[nodiscard]] static std::optional<QString> editionIsbn_path(const QJsonDocument& doc, int index)
{
    auto create_path = [](int idx)
    { return JSONPath::create(QString(u"$.inventory[%1].details.edition.isbn").arg(idx)); };

    auto evaluate         = [&doc](const JSONPath& path) { return path.evaluate(doc); };
    auto try_select_first = [](const QJsonArray& array) -> QJsonValue
    { return array.isEmpty() ? QJsonValue::Undefined : array.first(); };
    auto to_optional = [](const QString& s) -> std::optional<QString> { return s; };

    return create_path(index)
        .and_then(evaluate)
        .and_then(as<QJsonArray>)
        .transform(try_select_first)
        .and_then(as<QString>)
        .transform(to_optional)
        .value_or(std::nullopt);
}

// -----------------------------------------------------------------------------
// 2) Library implementation using JSONPath – concise & expressive
// -----------------------------------------------------------------------------
[[nodiscard]] static QStringList titlesAbovePrice_jsonquery(const QJsonDocument& doc, double threshold)
{
    const auto create_path = [&](double thr)
    { return JSONPath::create(QString(u"$.inventory[?(@.price > %1)].title").arg(thr)); };
    const auto evaluate = [&doc](const JSONPath& p)
    {
        return p.evaluate(doc); // expected<QJsonValue, Error>
    };
    const auto normalize_to_array = [](const QJsonValue& v) -> QJsonArray
    {
        if (v.isArray())
            return v.toArray();
        QJsonArray a;
        if (!v.isUndefined())
            a.append(v);
        return a;
    };
    // Nice name for the larger lambda
    const auto collect_titles_from_array = [](const QJsonArray& arr) -> QStringList
    {
        QStringList out;
        out.reserve(arr.size());
        for (const auto& item : arr)
            if (auto s = as<QString>(item))
                out << *s;
            else
                qWarning() << "Skipping non-string title:" << item << "Error:" << s.error().message_qt();
        return out;
    };

    return create_path(threshold)
        .transform_error(log_query_error("JSONPath creation failed"))
        .and_then(evaluate)
        .transform_error(log_query_error("JSONPath evaluation failed"))
        .transform(normalize_to_array)
        .transform(collect_titles_from_array)
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
    qInfo() << "\n=== ISBN Retrieval ===\n";
    for (const int idx : {0, 5, 7, 100})
    {
        qInfo() << "Book index:" << idx;

        // Plain version
        auto plainResult = editionIsbn_plain(doc, idx);
        qInfo().noquote() << "  Plain:   \"" << (plainResult ? *plainResult : "(not found)") << "\"";

        // Pointer version
        auto pointerResult = editionIsbn_pointer(doc, idx);
        qInfo().noquote() << "  Pointer: \"" << (pointerResult ? *pointerResult : "(not found)") << "\"";

        // Path version
        auto pathResult = editionIsbn_path(doc, idx);
        qInfo().noquote() << "  Path:    \"" << (pathResult ? *pathResult : "(not found)") << "\"";

        qInfo();
    }
    const int invalidIndex = 100;
    qDebug() << "\n=== Error Case (invalid index:" << invalidIndex << ") ===";
    const auto invalidPointer = editionIsbn_pointer(doc, invalidIndex);
    const auto invalidPath    = editionIsbn_path(doc, invalidIndex);

    qDebug() << "Pointer result:" << (invalidPointer ? *invalidPointer : "(not found)");
    qDebug() << "Path result:   " << (invalidPath ? *invalidPath : "(not found)");

    return 0;
}
