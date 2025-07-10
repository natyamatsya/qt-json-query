// benchmark.cpp
#include <QtTest/QtTest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QElapsedTimer>

#include "JSONPath.h"
#include "JSONPointer.hpp"

class BenchmarkTest : public QObject
{
    Q_OBJECT

private:
    QJsonDocument prepareTestDocument();

private slots:
    void initTestCase();

    // JSONPointer benchmarks
    void benchmarkJsonPointerSimplePath();
    void benchmarkJsonPointerNestedPath();
    void benchmarkJsonPointerArrayPath();
    void benchmarkJsonPointerComplexPath();

    // JSONPath benchmarks
    void benchmarkJsonPathSimplePath();
    void benchmarkJsonPathNestedPath();
    void benchmarkJsonPathArrayPath();
    void benchmarkJsonPathWildcard();
    void benchmarkJsonPathArraySlice();
    void benchmarkJsonPathFilter();
    void benchmarkJsonPathRecursive();

    // JSONPointer creation and parsing
    void benchmarkJsonPointerCreation();

    // JSONPath creation and parsing
    void benchmarkJsonPathCreation();
};

void BenchmarkTest::initTestCase()
{
    // This will run once before the first test function is executed
    qDebug() << "Starting benchmarks...";
}

QJsonDocument BenchmarkTest::prepareTestDocument()
{
    // Create a complex document with nested objects and arrays
    QJsonArray books = QJsonArray();
    for (int i = 0; i < 100; ++i)
    {
        books.append(QJsonObject{
            {"id", QString("book%1").arg(i)},
            {"title", QString("Book Title %1").arg(i)},
            {"author", QJsonObject{
                           {"name", QString("Author %1").arg(i % 10)},
                           {"year", 1900 + i % 100}}},
            {"price", 9.99 + (i % 20) * 0.5},
            {"categories", QJsonArray::fromVariantList({QString("category%1").arg(i % 5), QString("category%1").arg((i + 2) % 5), QString("category%1").arg((i + 4) % 5)})},
            {"inStock", i % 3 == 0}});
    }

    QJsonObject store{
        {"name", "Test Bookstore"},
        {"location", QJsonObject{
                         {"address", "123 Test Street"},
                         {"city", "Test City"},
                         {"state", "TS"},
                         {"zip", "12345"}}},
        {"inventory", books}};

    return QJsonDocument(store);
}

// JSONPointer benchmarks

void BenchmarkTest::benchmarkJsonPointerSimplePath()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPointer pointer("/name");
        QJsonValue result = pointer.evaluate(doc);
    }
}

void BenchmarkTest::benchmarkJsonPointerNestedPath()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPointer pointer("/location/city");
        QJsonValue result = pointer.evaluate(doc);
    }
}

void BenchmarkTest::benchmarkJsonPointerArrayPath()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPointer pointer("/inventory/5/title");
        QJsonValue result = pointer.evaluate(doc);
    }
}

void BenchmarkTest::benchmarkJsonPointerComplexPath()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPointer pointer("/inventory/15/author/name");
        QJsonValue result = pointer.evaluate(doc);
    }
}

// JSONPath benchmarks

void BenchmarkTest::benchmarkJsonPathSimplePath()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPath path("$.name");
        QJsonArray result = path.evaluate(doc);
    }
}

void BenchmarkTest::benchmarkJsonPathNestedPath()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPath path("$.location.city");
        QJsonArray result = path.evaluate(doc);
    }
}

void BenchmarkTest::benchmarkJsonPathArrayPath()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPath path("$.inventory[5].title");
        QJsonArray result = path.evaluate(doc);
    }
}

void BenchmarkTest::benchmarkJsonPathWildcard()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPath path("$.inventory[*].title");
        QJsonArray result = path.evaluate(doc);
    }
}

void BenchmarkTest::benchmarkJsonPathArraySlice()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPath path("$.inventory[0:10].title");
        QJsonArray result = path.evaluate(doc);
    }
}

void BenchmarkTest::benchmarkJsonPathFilter()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPath path("$.inventory[?(@.price > 20)].title");
        QJsonArray result = path.evaluate(doc);
    }
}

void BenchmarkTest::benchmarkJsonPathRecursive()
{
    QJsonDocument doc = prepareTestDocument();

    QBENCHMARK
    {
        JSONPath path("$..title");
        QJsonArray result = path.evaluate(doc);
    }
}

// Creation benchmarks

void BenchmarkTest::benchmarkJsonPointerCreation()
{
    QBENCHMARK
    {
        JSONPointer pointer("/inventory/25/categories/1");
    }
}

void BenchmarkTest::benchmarkJsonPathCreation()
{
    QBENCHMARK
    {
        JSONPath path("$.inventory[25].categories[1]");
    }
}

QTEST_MAIN(BenchmarkTest)
