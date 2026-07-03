// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// Counts global operator new calls around json_query entry points, answering
// "how many C++-side allocations do create/copy/evaluate perform?" (e.g. the
// pimpl unique_ptr, shared_ptr control blocks, internal vectors).
//
// Note: Qt containers allocate via malloc (QArrayData), not operator new, so
// these counts deliberately isolate the library's C++-side allocations from
// Qt's own container storage.
//
// Results feed the allocation table in perf/performance_baseline.md.

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <optional>

static std::atomic<size_t> g_newCalls{0};

void* operator new(std::size_t n)
{
    ++g_newCalls;
    if (void* p{std::malloc(n)})
        return p;
    std::abort();
}
void* operator new[](std::size_t n) { return operator new(n); }
void* operator new(std::size_t n, const std::nothrow_t&) noexcept
{
    ++g_newCalls;
    return std::malloc(n);
}
void* operator new[](std::size_t n, const std::nothrow_t&) noexcept
{
    ++g_newCalls;
    return std::malloc(n);
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include "json-query/JSONQuery"

using namespace json_query;

template <typename F>
static size_t countNews(const char* label, F&& f)
{
    const size_t before{g_newCalls.load()};
    f();
    const size_t delta{g_newCalls.load() - before};
    std::printf("%-52s %zu operator new calls\n", label, delta);
    return delta;
}

int main()
{
    const QJsonObject   store{{"books",
                               QJsonArray{
                                 QJsonObject{{"title", "Book 1"}, {"price", 10.0}},
                                 QJsonObject{{"title", "Book 2"}, {"price", 25.5}},
                             }}};
    const QJsonDocument doc{store};

    // Warm up Qt/CRT one-time allocations so they don't pollute the counts
    {
        auto warm{JSONPath::create(u"$.warmup")};
        (void)warm;
    }
    {
        auto warm{JSONPointer::create(u"/warmup")};
        (void)warm;
    }

    countNews("JSONPointer::create(\"/books/1/title\")",
              []
              {
                  auto p{JSONPointer::create(u"/books/1/title")};
                  (void)p;
              });

    std::optional<JSONPath::ParseResult> simple;
    countNews("JSONPath::create(\"$.books[*].title\") [no filter]",
              [&] { simple.emplace(JSONPath::create(u"$.books[*].title")); });

    std::optional<JSONPath::ParseResult> filter;
    countNews("JSONPath::create with filter [?(@.price > 12)]",
              [&] { filter.emplace(JSONPath::create(u"$.books[?(@.price > 12)].title")); });

    if (simple->has_value())
    {
        countNews("JSONPath copy construction [clones impl]",
                  [&]
                  {
                      JSONPath copy{**simple};
                      (void)copy;
                  });
        countNews("JSONPath::evaluate [no filter, small doc]",
                  [&]
                  {
                      auto r{(*simple)->evaluate(doc)};
                      (void)r;
                  });
    }
    if (filter->has_value())
    {
        countNews("JSONPath copy construction [filter path]",
                  [&]
                  {
                      JSONPath copy{**filter};
                      (void)copy;
                  });
        countNews("JSONPath::evaluate [filter, small doc]",
                  [&]
                  {
                      auto r{(*filter)->evaluate(doc)};
                      (void)r;
                  });
    }

    countNews("JSONSchema::create [small schema, shared_ptr]",
              []
              {
                  auto s{json_schema::JSONSchema::create(
                      QJsonObject{{"type", "object"}, {"required", QJsonArray{"books"}}})};
                  (void)s;
              });

    return 0;
}
