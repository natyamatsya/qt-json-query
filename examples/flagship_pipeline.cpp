// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// flagship_pipeline.cpp – The complete qt-json-query experience
//
// Scenario: An application receives a JSON API response containing employee
// records.  We validate the payload against a schema, query specific records
// with JSONPath filters, and extract typed C++ values — all composed into
// clean, monadic pipelines with automatic error propagation.
//
// Features demonstrated:
//   • JSONSchema   – compile-once validation
//   • JSONPath     – filter queries, recursive descent, evaluateSingle
//   • JSONPointer  – direct O(1) access by path
//   • as<T>        – type-safe conversions with pipe syntax
//   • Monadic ops  – and_then / transform / transform_error / value_or

#include "json-query/JSONQuery"

#include <QCoreApplication>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <expected>
#include <optional>
#include <vector>

using json_query::as;
using json_query::Error;
using json_query::JSONPath;
using json_query::JSONPointer;
using json_query::json_schema::JSONSchema;

// ─── Simulated API response ─────────────────────────────────────────────────

static QJsonObject apiResponse()
{
    return QJsonObject{
        {"version", "2.1"},
        {"department", "Engineering"},
        {"employees",
         QJsonArray{
             QJsonObject{{"name", "Alice Chen"},
                         {"role", "Senior Engineer"},
                         {"salary", 145000},
                         {"skills", QJsonArray{"C++", "Qt", "CMake"}},
                         {"active", true}},
             QJsonObject{{"name", "Bob Martinez"},
                         {"role", "Staff Engineer"},
                         {"salary", 185000},
                         {"skills", QJsonArray{"C++", "Rust", "Python"}},
                         {"active", true}},
             QJsonObject{{"name", "Carol Wang"},
                         {"role", "Junior Engineer"},
                         {"salary", 95000},
                         {"skills", QJsonArray{"Python", "JavaScript"}},
                         {"active", true}},
             QJsonObject{{"name", "Dave Kim"},
                         {"role", "Senior Engineer"},
                         {"salary", 155000},
                         {"skills", QJsonArray{"C++", "Qt", "Rust"}},
                         {"active", false}},
         }},
    };
}

// ─── Schema for the API response ────────────────────────────────────────────

static QJsonObject responseSchema()
{
    return QJsonObject{
        {"$schema", "https://json-schema.org/draft/2020-12/schema"},
        {"type", "object"},
        {"required", QJsonArray{"version", "department", "employees"}},
        {"properties",
         QJsonObject{
             {"version", QJsonObject{{"type", "string"}}},
             {"department", QJsonObject{{"type", "string"}}},
             {"employees",
              QJsonObject{
                  {"type", "array"},
                  {"items",
                   QJsonObject{
                       {"type", "object"},
                       {"required", QJsonArray{"name", "role", "salary", "active"}},
                       {"properties",
                        QJsonObject{
                            {"name", QJsonObject{{"type", "string"}, {"minLength", 1}}},
                            {"role", QJsonObject{{"type", "string"}}},
                            {"salary", QJsonObject{{"type", "integer"}, {"minimum", 0}}},
                            {"skills", QJsonObject{{"type", "array"}, {"items", QJsonObject{{"type", "string"}}}}},
                            {"active", QJsonObject{{"type", "boolean"}}},
                        }},
                   }},
              }},
         }},
    };
}

// ─── Helpers ────────────────────────────────────────────────────────────────

struct Employee
{
    QString     name;
    QString     role;
    int         salary;
    QStringList skills;
};

// Log an error and pass it through (for transform_error chains)
static auto logError(const QString& context)
{
    return [=](const Error& err)
    {
        qWarning().noquote() << "  [ERROR]" << context << ":" << err.message_qt();
        return err;
    };
}

// ─── Pipeline 1: Validate → Query → Convert ────────────────────────────────
//
// Validates the response, then finds all active employees earning > $100k
// using a single JSONPath filter, and converts each result to an Employee.

static std::vector<Employee> findHighEarners(const JSONSchema& schema, const QJsonValue& response)
{
    qDebug() << "\n── Pipeline 1: Active high earners (salary > 100k) ──";

    // Step 1: Validate
    const auto validation{schema.validate(response)};
    if (!validation)
    {
        qWarning() << "  Schema validation failed:";
        for (const auto& err : validation.errors())
            qWarning().noquote() << "    •" << err.instanceLocation << "—" << err.message;
        return {};
    }
    qDebug() << "  Schema validation passed";

    // Step 2: Query with JSONPath filter — one expression does the filtering
    const auto matches{JSONPath::create(u"$.employees[?(@.active == true && @.salary > 100000)]")
                           .and_then([&](const JSONPath& path) { return path.evaluate(response); })
                           .transform_error(logError("JSONPath query"))};

    if (!matches)
        return {};

    // Step 3: Convert each match to a typed Employee using as<T>
    std::vector<Employee> result;
    for (const auto& item : *matches)
    {
        const auto obj{item.toObject()};
        const auto name{obj["name"] | as<QString>};
        const auto role{obj["role"] | as<QString>};
        const auto salary{obj["salary"] | as<int>};

        if (name && role && salary)
        {
            QStringList skills;
            for (const auto& s : obj["skills"].toArray())
                if (const auto skill{s | as<QString>})
                    skills << *skill;

            result.push_back({*name, *role, *salary, skills});
        }
    }

    for (const auto& emp : result)
        qDebug().noquote()
            << QString("  %1 (%2) — $%3 — [%4]").arg(emp.name, emp.role).arg(emp.salary).arg(emp.skills.join(", "));

    return result;
}

// ─── Pipeline 2: Monadic one-liner with JSONPointer ─────────────────────────
//
// Directly access a specific field and convert to a typed value — no
// intermediate variables, pure monadic composition.

static void directAccess(const QJsonValue& response)
{
    qDebug() << "\n── Pipeline 2: Direct access via JSONPointer + as<T> ──";

    // Department name in one monadic chain
    const auto dept{JSONPointer::create(u"/department")
                        .and_then([&](const JSONPointer& ptr) { return ptr.evaluate(response); })
                        .and_then(as<QString>)
                        .transform_error(logError("department lookup"))};

    qDebug().noquote() << "  Department:" << dept.value_or("(unknown)");

    // First employee's name
    const auto firstName{JSONPointer::create(u"/employees/0/name")
                             .and_then([&](const JSONPointer& ptr) { return ptr.evaluate(response); })
                             .and_then(as<QString>)
                             .transform_error(logError("first employee lookup"))};

    qDebug().noquote() << "  First employee:" << firstName.value_or("(unknown)");

    // Salary of third employee (index 2)
    const auto salary{JSONPointer::create(u"/employees/2/salary")
                          .and_then([&](const JSONPointer& ptr) { return ptr.evaluate(response); })
                          .and_then(as<int>)
                          .transform_error(logError("salary lookup"))};

    if (salary)
        qDebug().noquote() << QString("  Third employee salary: $%1").arg(*salary);
}

// ─── Pipeline 3: Recursive descent + evaluateSingle ─────────────────────────
//
// Find all skills across all employees using recursive descent, then
// extract a single scalar value with evaluateSingle.

static void aggregateQueries(const QJsonValue& response)
{
    qDebug() << "\n── Pipeline 3: Recursive descent + evaluateSingle ──";

    // All skills across all employees
    const auto allSkills{JSONPath::create(u"$.employees[*].skills[*]")
                             .and_then([&](const JSONPath& path) { return path.evaluate(response); })
                             .transform_error(logError("skills query"))};

    if (allSkills)
    {
        QStringList unique;
        for (const auto& s : *allSkills)
            if (const auto skill{s | as<QString>}; skill && !unique.contains(*skill))
                unique << *skill;

        qDebug().noquote() << "  All unique skills:" << unique.join(", ");
    }

    // evaluateSingle — get exactly one value, no array wrapping
    const auto version{JSONPath::create(u"$.version")
                           .and_then([&](const JSONPath& path) { return path.evaluateSingle(response); })
                           .and_then(as<QString>)
                           .transform_error(logError("version query"))};

    qDebug().noquote() << "  API version:" << version.value_or("(unknown)");
}

// ─── Pipeline 4: Schema validation catches bad data ─────────────────────────

static void validateBadData(const JSONSchema& schema)
{
    qDebug() << "\n── Pipeline 4: Schema rejects invalid payload ──";

    const auto badResponse{QJsonObject{
        {"version", 42},      // should be string
        {"department", true}, // should be string
        // missing "employees" entirely
    }};

    const auto result{schema.validate(QJsonValue{badResponse})};
    if (result)
    {
        qDebug() << "  Unexpectedly valid!";
    }
    else
    {
        qDebug().noquote() << QString("  Caught %1 error(s):").arg(result.errorCount());
        for (const auto& err : result.errors())
            qDebug().noquote() << "    •" << err.instanceLocation << "—" << err.message;
    }
}

// ─── Main ───────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "═══════════════════════════════════════════════════";
    qDebug() << " qt-json-query: Flagship Pipeline Demo";
    qDebug() << "═══════════════════════════════════════════════════";

    // Compile the schema once — reuse for every request
    const auto schema{JSONSchema::create(responseSchema())};
    if (!schema)
    {
        qCritical() << "Schema compilation failed:" << schema.error().message_qt();
        return 1;
    }
    qDebug() << "Schema compiled successfully";

    const auto response{QJsonValue{apiResponse()}};

    findHighEarners(*schema, response);
    directAccess(response);
    aggregateQueries(response);
    validateBadData(*schema);

    qDebug() << "\n═══════════════════════════════════════════════════";

    return 0;
}
