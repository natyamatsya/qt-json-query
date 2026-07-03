// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

// compile_once_reuse.cpp – Compile paths and schemas once, apply to many documents
//
// Scenario: A log analytics service receives structured JSON events from
// multiple microservices.  At startup it compiles the extraction paths and
// validation schema once, then reuses them to process every incoming event.
//
// Key takeaway: JSONPath::create, JSONPointer::create, and JSONSchema::create
// are the expensive operations — they parse, validate, and optimise the
// expression.  The resulting objects are immutable and can be shared freely
// across threads and applied to millions of documents without re-parsing.

#include "json-query/JSONQuery"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <vector>

using json_query::as;
using json_query::Error;
using json_query::JSONPath;
using json_query::JSONPointer;
using json_query::json_schema::JSONSchema;

// ─── Pre-compiled query toolkit ─────────────────────────────────────────────
//
// In production this would live in a class initialised at startup.  The paths
// and schema are parsed exactly once and stored as const members.

struct EventProcessor
{
    // Schema: every event must have timestamp, service, level, message
    JSONSchema schema;

    // Fast O(1) access for fixed fields
    JSONPointer timestampPtr;
    JSONPointer servicePtr;
    JSONPointer levelPtr;
    JSONPointer messagePtr;

    // JSONPath query to find all error events with high severity
    JSONPath errorFilter;

    // Factory — compile everything once, fail fast on bad expressions
    static std::expected<EventProcessor, Error> create()
    {
        const auto schemaJson{QJsonObject{
            {"$schema", "https://json-schema.org/draft/2020-12/schema"},
            {"type", "object"},
            {"required", QJsonArray{"timestamp", "service", "level", "message"}},
            {"properties",
             QJsonObject{
                 {"timestamp", QJsonObject{{"type", "string"}}},
                 {"service", QJsonObject{{"type", "string"}, {"minLength", 1}}},
                 {"level",
                  QJsonObject{{"type", "string"}, {"enum", QJsonArray{"debug", "info", "warn", "error", "fatal"}}}},
                 {"message", QJsonObject{{"type", "string"}}},
                 {"metadata", QJsonObject{{"type", "object"}}},
             }},
        }};

        auto schema{JSONSchema::create(schemaJson)};
        if (!schema)
            return std::unexpected(schema.error());

        auto tsPtr{JSONPointer::create(u"/timestamp")};
        if (!tsPtr)
            return std::unexpected(tsPtr.error());

        auto svcPtr{JSONPointer::create(u"/service")};
        if (!svcPtr)
            return std::unexpected(svcPtr.error());

        auto lvlPtr{JSONPointer::create(u"/level")};
        if (!lvlPtr)
            return std::unexpected(lvlPtr.error());

        auto msgPtr{JSONPointer::create(u"/message")};
        if (!msgPtr)
            return std::unexpected(msgPtr.error());

        auto errFilter{JSONPath::create(u"$[?(@.level == 'error' || @.level == 'fatal')]")};
        if (!errFilter)
            return std::unexpected(errFilter.error());

        return EventProcessor{
            std::move(*schema),
            std::move(*tsPtr),
            std::move(*svcPtr),
            std::move(*lvlPtr),
            std::move(*msgPtr),
            std::move(*errFilter),
        };
    }
};

// ─── Simulated event stream ─────────────────────────────────────────────────

static std::vector<QJsonObject> simulatedEvents()
{
    return {
        {{"timestamp", "2026-02-08T22:01:00Z"},
         {"service", "auth"},
         {"level", "info"},
         {"message", "User login successful"},
         {"metadata", QJsonObject{{"user_id", "u-1234"}}}},
        {{"timestamp", "2026-02-08T22:01:05Z"},
         {"service", "payment"},
         {"level", "error"},
         {"message", "Payment gateway timeout"},
         {"metadata", QJsonObject{{"tx_id", "tx-5678"}, {"retry", 3}}}},
        {{"timestamp", "2026-02-08T22:01:10Z"},
         {"service", "auth"},
         {"level", "warn"},
         {"message", "Rate limit approaching"},
         {"metadata", QJsonObject{{"requests_per_min", 450}}}},
        {{"timestamp", "2026-02-08T22:01:15Z"},
         {"service", "search"},
         {"level", "debug"},
         {"message", "Cache miss for query 'widgets'"}},
        {{"timestamp", "2026-02-08T22:01:20Z"},
         {"service", "payment"},
         {"level", "fatal"},
         {"message", "Database connection pool exhausted"}},
        {{"timestamp", "2026-02-08T22:01:25Z"},
         {"service", "auth"},
         {"level", "info"},
         {"message", "Token refreshed"},
         {"metadata", QJsonObject{{"user_id", "u-1234"}}}},
        // Invalid event — missing required "level" field
        {{"timestamp", "2026-02-08T22:01:30Z"}, {"service", "billing"}, {"message", "Invoice generated"}},
    };
}

// ─── Processing ─────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    QCoreApplication app{argc, argv};

    // ── Step 1: Compile once at startup ──────────────────────────────
    qDebug() << "Compiling paths, schema, and filters...";

    const auto processor{EventProcessor::create()};
    if (!processor)
    {
        qCritical() << "Failed to initialise processor:" << processor.error().message_qt();
        return 1;
    }
    qDebug() << "Ready.\n";

    const auto events{simulatedEvents()};

    // ── Step 2: Validate & extract from every event ──────────────────
    qDebug() << "Processing" << events.size() << "events...\n";

    int valid{0};
    int invalid{0};

    for (const auto& event : events)
    {
        const auto eventValue{QJsonValue{event}};

        // Validate against the pre-compiled schema
        const auto result{processor->schema.validate(eventValue)};
        if (!result)
        {
            ++invalid;
            const auto svc{processor->servicePtr.evaluate(eventValue).and_then(as<QString>)};
            qDebug().noquote() << QString("  REJECTED [%1]: %2").arg(svc.value_or("?"), result.firstError());
            continue;
        }

        ++valid;

        // Extract fields using pre-compiled pointers — no re-parsing
        const auto ts{processor->timestampPtr.evaluate(eventValue).and_then(as<QString>)};
        const auto svc{processor->servicePtr.evaluate(eventValue).and_then(as<QString>)};
        const auto lvl{processor->levelPtr.evaluate(eventValue).and_then(as<QString>)};
        const auto msg{processor->messagePtr.evaluate(eventValue).and_then(as<QString>)};

        qDebug().noquote() << QString("  [%1] %2/%3: %4")
                                  .arg(ts.value_or("?"), svc.value_or("?"), lvl.value_or("?"), msg.value_or("?"));
    }

    qDebug().noquote() << QString("\nProcessed: %1 valid, %2 rejected").arg(valid).arg(invalid);

    // ── Step 3: Query across the whole batch ─────────────────────────
    //
    // Wrap all events in an array and apply the pre-compiled error filter.
    // This shows a JSONPath with a filter predicate reused on a batch.

    qDebug() << "\n── Critical events (error + fatal) ──";

    QJsonArray allEvents;
    for (const auto& e : events)
        allEvents.append(e);

    const auto critical{processor->errorFilter.evaluate(QJsonValue{allEvents})};
    if (critical)
    {
        for (const auto& item : *critical)
        {
            const auto obj{item.toObject()};
            const auto svc{obj["service"] | as<QString>};
            const auto msg{obj["message"] | as<QString>};
            const auto lvl{obj["level"] | as<QString>};
            qDebug().noquote()
                << QString("  [%1] %2: %3").arg(lvl.value_or("?"), svc.value_or("?"), msg.value_or("?"));
        }
    }

    return 0;
}
