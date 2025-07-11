// benchmark.cpp - Google Benchmark version
#include <benchmark/benchmark.h>
#include <QJsonDocument>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>

#include "json-query/JSONPath.hpp"
#include "json-query/JSONPointer.hpp"

// Helper to build a moderately large test document
static QJsonDocument prepareTestDocument()
{
    QJsonArray books;
    for (int i = 0; i < 100; ++i)
    {
        books.append(QJsonObject{{"id", QString("book%1").arg(i)},
                                 {"title", QString("Book Title %1").arg(i)},
                                 {"author", QJsonObject{{"name", QString("Author %1").arg(i % 10)}, {"year", 1900 + i % 100}}},
                                 {"price", 9.99 + (i % 20) * 0.5},
                                 {"categories", QJsonArray::fromStringList({QString("cat%1").arg(i % 5), QString("cat%1").arg((i + 1) % 5)})},
                                 {"inStock", i % 3 == 0}});
    }

    QJsonObject store{{"name", "Test Bookstore"},
                      {"location", QJsonObject{{"city", "Test City"}}},
                      {"inventory", books}};
    return QJsonDocument(store);
}

// ----------------------------
// JSONPointer benchmarks
// ----------------------------
static void BM_JSONPointer_Simple(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        JSONPointer ptr("/name");
        benchmark::DoNotOptimize(ptr.evaluate(doc));
    }
}
BENCHMARK(BM_JSONPointer_Simple);

static void BM_JSONPointer_Nested(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        JSONPointer ptr("/location/city");
        benchmark::DoNotOptimize(ptr.evaluate(doc));
    }
}
BENCHMARK(BM_JSONPointer_Nested);

static void BM_JSONPointer_Array(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        JSONPointer ptr("/inventory/5/title");
        benchmark::DoNotOptimize(ptr.evaluate(doc));
    }
}
BENCHMARK(BM_JSONPointer_Array);

static void BM_JSONPointer_Complex(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        JSONPointer ptr("/inventory/15/author/name");
        benchmark::DoNotOptimize(ptr.evaluate(doc));
    }
}
BENCHMARK(BM_JSONPointer_Complex);

// ----------------------------
// JSONPath benchmarks
// ----------------------------
static void BM_JSONPath_Simple(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        JSONPath path("$.name");
        benchmark::DoNotOptimize(path.evaluate(doc));
    }
}
BENCHMARK(BM_JSONPath_Simple);

static void BM_JSONPath_Nested(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        JSONPath path("$.location.city");
        benchmark::DoNotOptimize(path.evaluate(doc));
    }
}
BENCHMARK(BM_JSONPath_Nested);

static void BM_JSONPath_Array(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        JSONPath path("$.inventory[5].title");
        benchmark::DoNotOptimize(path.evaluate(doc));
    }
}
BENCHMARK(BM_JSONPath_Array);

static void BM_JSONPath_Filter(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        JSONPath path("$.inventory[?(@.price > 20)].title");
        benchmark::DoNotOptimize(path.evaluate(doc));
    }
}
BENCHMARK(BM_JSONPath_Filter);

static void BM_JSONPath_Recursive(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        JSONPath path("$..title");
        benchmark::DoNotOptimize(path.evaluate(doc));
    }
}
BENCHMARK(BM_JSONPath_Recursive);

// Creation benchmarks
static void BM_JSONPointer_Creation(benchmark::State &state)
{
    for (auto _ : state)
    {
        JSONPointer ptr("/inventory/25/categories/1");
        benchmark::DoNotOptimize(ptr);
    }
}
BENCHMARK(BM_JSONPointer_Creation);

static void BM_JSONPath_Creation(benchmark::State &state)
{
    for (auto _ : state)
    {
        JSONPath path("$.inventory[25].categories[1]");
        benchmark::DoNotOptimize(path);
    }
}
BENCHMARK(BM_JSONPath_Creation);

BENCHMARK_MAIN();

#if 0
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

()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPointer pointer("/name");
        QJsonValue result = pointer.evaluate(doc);
    }
}

benchmarkJsonPointerNestedPath()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPointer pointer("/location/city");
        QJsonValue result = pointer.evaluate(doc);
    }
}

benchmarkJsonPointerArrayPath()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPointer pointer("/inventory/5/title");
        QJsonValue result = pointer.evaluate(doc);
    }
}

benchmarkJsonPointerComplexPath()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPointer pointer("/inventory/15/author/name");
        QJsonValue result = pointer.evaluate(doc);
    }
}

// JSONPath benchmarks

()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.name");
        QJsonArray result = path.evaluate(doc);
    }
}

benchmarkJsonPathNestedPath()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.location.city");
        QJsonArray result = path.evaluate(doc);
    }
}

benchmarkJsonPathArrayPath()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.inventory[5].title");
        QJsonArray result = path.evaluate(doc);
    }
}

benchmarkJsonPathWildcard()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.inventory[*].title");
        QJsonArray result = path.evaluate(doc);
    }
}

benchmarkJsonPathArraySlice()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.inventory[0:10].title");
        QJsonArray result = path.evaluate(doc);
    }
}

benchmarkJsonPathFilter()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.inventory[?(@.price > 20)].title");
        QJsonArray result = path.evaluate(doc);
    }
}

benchmarkJsonPathRecursive()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$..title");
        QJsonArray result = path.evaluate(doc);
    }
}

// Creation benchmarks

()
{
    
    {
        JSONPointer pointer("/inventory/25/categories/1");
    }
}

benchmarkJsonPathCreation()
{
    
    {
        JSONPath path("$.inventory[25].categories[1]");
    }
}
#endif
