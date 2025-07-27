// benchmark.cpp - Google Benchmark version
#include <benchmark/benchmark.h>
#include <QJsonDocument>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringView>

#include "json-query/json-path/JSONPath.hpp"
#include "json-query/json-pointer/JSONPointer.hpp"

#include <utility>

using json_query::JSONPath;
using json_query::JSONPointer;
using namespace Qt::StringLiterals;

// Helper to build a moderately large test document for benchmarking
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
        auto ptr{JSONPointer::create(QStringLiteral("/name"))};
        auto res{ptr->evaluate(doc)};
        if (!res) {
            benchmark::DoNotOptimize(QJsonValue{});
        } else {
            benchmark::DoNotOptimize(*res);
        }
    }
}
BENCHMARK(BM_JSONPointer_Simple);

static void BM_JSONPointer_Nested(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        auto ptr{JSONPointer::create(QStringLiteral("/location/city"))};
        auto result{ptr->evaluate(doc)};
        if (result) {
            benchmark::DoNotOptimize(*result);
        }
    }
}
BENCHMARK(BM_JSONPointer_Nested);

static void BM_JSONPointer_Array(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        auto ptr{JSONPointer::create(QStringLiteral("/inventory/5/title"))};
        auto result{ptr->evaluate(doc)};
        if (result) {
            benchmark::DoNotOptimize(*result);
        }
    }
}
BENCHMARK(BM_JSONPointer_Array);

static void BM_JSONPointer_Complex(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        auto ptr{JSONPointer::create(QStringLiteral("/inventory/15/author/name"))};
        auto result{ptr->evaluate(doc)};
        if (result) {
            benchmark::DoNotOptimize(*result);
        }
    }
}
BENCHMARK(BM_JSONPointer_Complex);

// ----------------------------
// Plain QJson benchmarks (manual traversal)
// ----------------------------
static void BM_Plain_Simple(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        const auto root = doc.object();
        benchmark::DoNotOptimize(root.value("name").toString());
    }
}
BENCHMARK(BM_Plain_Simple);

static void BM_Plain_Nested(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        const auto root = doc.object();
        const auto city = root.value("location").toObject().value("city").toString();
        benchmark::DoNotOptimize(city.constData());
    }
}
BENCHMARK(BM_Plain_Nested);

static void BM_Plain_Array(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        const auto inv = doc.object().value("inventory").toArray();
        const auto title = inv.at(5).toObject().value("title").toString();
        benchmark::DoNotOptimize(title.constData());
    }
}
BENCHMARK(BM_Plain_Array);

static void BM_Plain_Filter(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        QStringList titles;
        const auto inv = doc.object().value("inventory").toArray();
        for (const QJsonValue &v : inv)
        {
            const auto o = v.toObject();
            if (o.value("price").toDouble() > 20)
                titles << o.value("title").toString();
        }
        benchmark::DoNotOptimize(titles);
    }
}
BENCHMARK(BM_Plain_Filter);

static void BM_Plain_Recursive(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        QStringList titles;
        const auto inv = doc.object().value("inventory").toArray();
        for (const QJsonValue &v : inv)
        {
            const auto t = v.toObject().value("title").toString();
            titles << t;
        }
        benchmark::DoNotOptimize(titles);
    }
}
BENCHMARK(BM_Plain_Recursive);

// ----------------------------
// JSONPath benchmarks
// ----------------------------
static void BM_JSONPath_Simple(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        auto pathRes{JSONPath::create(u"$.name")};
        if (!pathRes.has_value())
            state.SkipWithError("Failed to compile path");
        auto result{pathRes->evaluateAll(doc)};
        if (result) {
            benchmark::DoNotOptimize(*result);
        }
    }
}
BENCHMARK(BM_JSONPath_Simple);

static void BM_JSONPath_Nested(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        auto p{JSONPath::create(u"$.location.city")};
        if (!p) state.SkipWithError("compile fail");
        auto result{p->evaluateAll(doc)};
        if (result) {
            benchmark::DoNotOptimize(*result);
        }
    }
}
BENCHMARK(BM_JSONPath_Nested);

static void BM_JSONPath_Array(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        auto p{JSONPath::create(u"$.inventory[5].title")};
        if (!p) state.SkipWithError("compile fail");
        auto result{p->evaluateAll(doc)};
        if (result) {
            benchmark::DoNotOptimize(*result);
        }
    }
}
BENCHMARK(BM_JSONPath_Array);

static void BM_JSONPath_Filter(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        auto p = JSONPath::create(u"$.inventory[?(@.price > 20)].title");
        if (!p) state.SkipWithError("compile fail");
        auto result{p->evaluateAll(doc)};
        if (result) {
            benchmark::DoNotOptimize(*result);
        }
    }
}
BENCHMARK(BM_JSONPath_Filter);

static void BM_JSONPath_Recursive(benchmark::State &state)
{
    QJsonDocument doc = prepareTestDocument();
    for (auto _ : state)
    {
        auto p{JSONPath::create(u"$..title")};
        if (!p) state.SkipWithError("compile fail");
        auto result{p->evaluateAll(doc)};
        if (result) {
            benchmark::DoNotOptimize(*result);
        }
    }
}
BENCHMARK(BM_JSONPath_Recursive);

// Creation benchmarks
static void BM_JSONPointer_Creation(benchmark::State &state)
{
    for (auto _ : state)
    {
        auto ptr{JSONPointer::create(QStringLiteral("/inventory/25/categories/1"))};
        benchmark::DoNotOptimize(ptr);
    }
}
BENCHMARK(BM_JSONPointer_Creation);

static void BM_JSONPath_Creation(benchmark::State &state)
{
    for (auto _ : state)
    {
        benchmark::DoNotOptimize(JSONPath::create(u"$.inventory[25].categories[1]"));
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
            {"inStock", i % 3 == 0}};
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
        auto res{pointer.evaluate(doc)};
        benchmark::DoNotOptimize(res);
    }
}

benchmarkJsonPointerNestedPath()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPointer pointer("/location/city");
        benchmark::DoNotOptimize(pointer.evaluate(doc));
    }
}

benchmarkJsonPointerArrayPath()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPointer pointer("/inventory/5/title");
        benchmark::DoNotOptimize(pointer.evaluate(doc));
    }
}

benchmarkJsonPointerComplexPath()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPointer pointer("/inventory/15/author/name");
        benchmark::DoNotOptimize(pointer.evaluate(doc));
    }
}

// JSONPath benchmarks

()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.name");
        auto result = path.evaluate(doc);
    }
}

benchmarkJsonPathNestedPath()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.location.city");
        auto result = path.evaluate(doc);
    }
}

benchmarkJsonPathArrayPath()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.inventory[5].title");
        auto result = path.evaluate(doc);
    }
}

benchmarkJsonPathWildcard()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.inventory[*].title");
        auto result = path.evaluate(doc);
    }
}

benchmarkJsonPathArraySlice()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.inventory[0:10].title");
        auto result = path.evaluate(doc);
    }
}

benchmarkJsonPathFilter()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$.inventory[?(@.price > 20)].title");
        auto result = path.evaluate(doc);
    }
}

benchmarkJsonPathRecursive()
{
    QJsonDocument doc = prepareTestDocument();

    
    {
        JSONPath path("$..title");
        auto result = path.evaluate(doc);
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
