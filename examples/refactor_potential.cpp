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

using json_query::JSONPath;
using json_query::JSONPointer;
using json_query::QueryError;

// Custom error type for JSON value conversion
template <typename T>
struct ConversionError
{
    QString          message;
    QJsonValue::Type expectedType;
    QJsonValue::Type actualType;
};

// Type-safe conversion from QJsonValue to T
template <typename T>
std::expected<T, ConversionError<T>> as(const QJsonValue& value);

// Specialization for QString
template <>
std::expected<QString, ConversionError<QString>> as<QString>(const QJsonValue& value)
{
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toDouble());
    if (value.isBool())
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    if (value.isNull() || value.isUndefined())
    {
        return std::unexpected(
            ConversionError<QString>{"Cannot convert null/undefined to QString", QJsonValue::String, value.type()});
    }
    if (value.isArray() || value.isObject())
    {
        QJsonDocument doc = value.isArray() ? QJsonDocument(value.toArray()) : QJsonDocument(value.toObject());
        return doc.toJson(QJsonDocument::Compact);
    }

    return std::unexpected(
        ConversionError<QString>{"Unsupported type for QString conversion", QJsonValue::String, value.type()});
}

// Specialization for int
template <>
std::expected<int, ConversionError<int>> as<int>(const QJsonValue& value)
{
    if (value.isDouble())
    {
        return static_cast<int>(value.toDouble());
    }
    else if (value.isString())
    {
        bool ok;
        int  result = value.toString().toInt(&ok);
        if (ok)
            return result;
    }
    else if (value.isBool())
    {
        return value.toBool() ? 1 : 0;
    }

    return std::unexpected(ConversionError<int>{"Cannot convert to int", QJsonValue::Double, value.type()});
}

// Specialization for double
template <>
std::expected<double, ConversionError<double>> as<double>(const QJsonValue& value)
{
    if (value.isDouble())
    {
        return value.toDouble();
    }
    else if (value.isString())
    {
        bool   ok;
        double result = value.toString().toDouble(&ok);
        if (ok)
            return result;
    }
    else if (value.isBool())
    {
        return value.toBool() ? 1.0 : 0.0;
    }

    return std::unexpected(ConversionError<double>{"Cannot convert to double", QJsonValue::Double, value.type()});
}

// Specialization for bool
template <>
std::expected<bool, ConversionError<bool>> as<bool>(const QJsonValue& value)
{
    if (value.isBool())
    {
        return value.toBool();
    }
    else if (value.isDouble())
    {
        return value.toDouble() != 0.0;
    }
    else if (value.isString())
    {
        const QString str = value.toString().toLower();
        if (str == "true" || str == "1")
            return true;
        if (str == "false" || str == "0")
            return false;
    }

    return std::unexpected(ConversionError<bool>{"Cannot convert to bool", QJsonValue::Bool, value.type()});
}

// Specialization for QJsonArray
template <>
std::expected<QJsonArray, ConversionError<QJsonArray>> as<QJsonArray>(const QJsonValue& value)
{
    if (value.isArray())
        return value.toArray();

    return std::unexpected(ConversionError<QJsonArray>{"Value is not an array", QJsonValue::Array, value.type()});
}

// Specialization for QJsonObject
template <>
std::expected<QJsonObject, ConversionError<QJsonObject>> as<QJsonObject>(const QJsonValue& value)
{
    if (value.isObject())
        return value.toObject();

    return std::unexpected(ConversionError<QJsonObject>{"Value is not an object", QJsonValue::Object, value.type()});
}

// Helper function to convert QJsonValue to QString with error checking using as<T>
static std::optional<QString> jsonValueToString(const QJsonValue& value)
{
    auto result = as<QString>(value);
    if (result)
        return *result;
    qWarning() << "Failed to convert JSON value to string:" << result.error().message;
    return std::nullopt;
}

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
    auto result = JSONPointer::create(QString("/inventory/%1/details/edition/isbn").arg(index));
    if (!result)
    {
        qWarning() << "Failed to create JSONPointer";
        return std::nullopt;
    }

    auto evalResult = result->evaluate(doc);
    if (!evalResult)
    {
        qWarning() << "Failed to evaluate JSONPointer";
        return std::nullopt;
    }

    // Use the as<T> function for type-safe conversion
    auto strResult = as<QString>(*evalResult);
    if (!strResult)
    {
        qWarning() << "Failed to convert to string:" << strResult.error().message;
        return std::nullopt;
    }

    return *strResult;
}

[[nodiscard]] static std::optional<QString> editionIsbn_path(const QJsonDocument& doc, int index)
{
    auto path = JSONPath::create(QString(u"$.inventory[%1].details.edition.isbn").arg(index));
    if (!path)
    {
        qWarning() << "Failed to create JSONPath";
        return std::nullopt;
    }

    auto evalResult = path->evaluate(doc);
    if (!evalResult)
    {
        qWarning() << "Failed to evaluate JSONPath";
        return std::nullopt;
    }

    // Handle array result (JSONPath can return arrays)
    auto arrResult = as<QJsonArray>(*evalResult);
    if (arrResult)
    {
        if (arrResult->isEmpty())
        {
            qWarning() << "Empty array result from JSONPath";
            return std::nullopt;
        }
        return as<QString>(arrResult->first())
            .transform([](const QString& s) { return std::optional<QString>(s); })
            .value_or(std::nullopt);
    }

    // Handle single value result
    return as<QString>(*evalResult)
        .transform([](const QString& s) { return std::optional<QString>(s); })
        .value_or(std::nullopt);
}

// -----------------------------------------------------------------------------
// 2) Library implementation using JSONPath – concise & expressive
// -----------------------------------------------------------------------------
[[nodiscard]] static QStringList titlesAbovePrice_jsonquery(const QJsonDocument& doc, double threshold)
{
    auto path = JSONPath::create(QString("$.inventory[?(@.price > %1)].title").arg(threshold));
    if (!path)
    {
        qWarning() << "Failed to create JSONPath";
        return {};
    }

    auto evalResult = path->evaluate(doc);
    if (!evalResult)
    {
        qWarning() << "Failed to evaluate JSONPath";
        return {};
    }

    QStringList result;

    // Handle array result
    auto arrResult = as<QJsonArray>(*evalResult);
    if (arrResult)
    {
        for (const auto& item : *arrResult)
        {
            auto strResult = as<QString>(item);
            if (strResult)
                result << *strResult;
            else
                qWarning() << "Skipping non-string title:" << item << "Error:" << strResult.error().message;
        }
    }
    // Handle single value result
    else if (!evalResult->isUndefined())
    {
        auto strResult = as<QString>(*evalResult);
        if (strResult)
        {
            result << *strResult;
        }
        else
        {
            qWarning() << "Expected string title but got type:" << evalResult->type()
                       << "Error:" << strResult.error().message;
        }
    }

    return result;
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
