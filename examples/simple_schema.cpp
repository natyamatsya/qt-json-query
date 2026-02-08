// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/JSONQuery"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

using namespace json_query;
using namespace json_query::json_schema;

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    // ── 1. Define a JSON Schema ─────────────────────────────────────
    const auto schemaJson{QJsonObject{
        {"$schema", "https://json-schema.org/draft/2020-12/schema"},
        {"type", "object"},
        {"required", QJsonArray{"name", "age", "email"}},
        {"properties",
         QJsonObject{
             {"name",
              QJsonObject{
                  {"type", "string"},
                  {"minLength", 1},
              }},
             {"age",
              QJsonObject{
                  {"type", "integer"},
                  {"minimum", 0},
                  {"maximum", 150},
              }},
             {"email",
              QJsonObject{
                  {"type", "string"},
                  {"format", "email"},
              }},
             {"tags",
              QJsonObject{
                  {"type", "array"},
                  {"items", QJsonObject{{"type", "string"}}},
                  {"uniqueItems", true},
              }},
         }},
        {"additionalProperties", false},
    }};

    // ── 2. Compile the schema (once, reuse many times) ──────────────
    auto schema{JSONSchema::create(schemaJson)};
    if (!schema)
    {
        qCritical() << "Schema compilation failed:" << schema.error().formatted_message();
        return 1;
    }
    qDebug() << "Schema compiled successfully.\n";

    // ── 3. Validate a VALID instance ────────────────────────────────
    const auto validPerson{QJsonObject{
        {"name", "Alice"},
        {"age", 30},
        {"email", "alice@example.com"},
        {"tags", QJsonArray{"developer", "speaker"}},
    }};

    auto result1{schema->validate(QJsonValue{validPerson})};
    qDebug() << "=== Valid person ===";
    if (result1)
        qDebug() << "  Result: VALID\n";
    else
        qDebug() << "  Result: INVALID (" << result1.errorCount() << "errors)\n";

    // ── 4. Validate an INVALID instance ─────────────────────────────
    const auto invalidPerson{QJsonObject{
        {"name", ""},                   // violates minLength: 1
        {"age", -5},                    // violates minimum: 0
                                        // missing "email" (required)
        {"tags", QJsonArray{"a", "a"}}, // violates uniqueItems
        {"extra", true},                // violates additionalProperties: false
    }};

    auto result2{schema->validate(QJsonValue{invalidPerson})};
    qDebug() << "=== Invalid person ===";
    if (result2)
    {
        qDebug() << "  Result: VALID (unexpected!)\n";
    }
    else
    {
        qDebug() << "  Result: INVALID (" << result2.errorCount() << "errors)";
        for (const auto& err : result2.errors())
            qDebug().noquote() << "  •" << err.instanceLocation << "—" << err.message;
        qDebug() << "";
    }

    // ── 5. Quick validation (bool only, no error details) ───────────
    qDebug() << "=== Quick validation ===";
    qDebug() << "  Valid person:  " << schema->isValid(QJsonValue{validPerson});
    qDebug() << "  Invalid person:" << schema->isValid(QJsonValue{invalidPerson});

    return 0;
}
