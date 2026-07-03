// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

/**
 * @file fuzz_jsonschema.cpp
 * @brief LibFuzzer target for JSON Schema compilation and validation
 *
 * Fuzzes the complete schema pipeline: a fuzzed schema document is compiled
 * (exercising $ref resolution, keyword compilation, regex compilation, and
 * the unresolved-ref policy) and, when compilation succeeds, a fuzzed
 * instance is validated against it (exercising all keyword validators and
 * the $ref-cycle guard).
 */

#include <cstddef>
#include <cstdint>
#include <algorithm>

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonValue>

#include <json-query/JSONQuery>

using namespace json_query::json_schema;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    if (size < 2)
        return 0;

    // Split input: first byte determines split point (schema | instance)
    const size_t splitPoint = data[0] % (size - 1) + 1;

    const QByteArray schemaInput(reinterpret_cast<const char*>(data + 1),
                                 static_cast<int>(std::min(splitPoint, size - 1)));
    const QByteArray instanceInput(reinterpret_cast<const char*>(data + splitPoint + 1),
                                   static_cast<int>(size - splitPoint - 1));

    QJsonParseError parseError{};
    const auto      schemaDoc = QJsonDocument::fromJson(schemaInput, &parseError);
    if (parseError.error != QJsonParseError::NoError || schemaDoc.isNull())
        return 0;

    const auto schemaValue = schemaDoc.isObject() ? QJsonValue{schemaDoc.object()} : QJsonValue{schemaDoc.array()};

    // Compile under both format modes and both unresolved-ref policies
    SchemaOptions strict{};
    strict.formatValidation    = FormatValidation::Assertion;
    strict.unresolvedRefPolicy = UnresolvedRefPolicy::Fail;

    auto lenientSchema = JSONSchema::create(schemaValue);
    auto strictSchema  = JSONSchema::create(schemaValue, nullptr, strict);

    const auto instanceDoc = QJsonDocument::fromJson(instanceInput, &parseError);

    for (auto* schema : {&lenientSchema, &strictSchema})
    {
        if (!*schema)
            continue;

        // Validate the schema against itself (cheap self-referential input)
        static_cast<void>((*schema)->isValid(schemaValue));

        if (parseError.error == QJsonParseError::NoError && !instanceDoc.isNull())
        {
            const auto instance =
                instanceDoc.isObject() ? QJsonValue{instanceDoc.object()} : QJsonValue{instanceDoc.array()};
            const auto result = (*schema)->validate(instance);
            // Exercise error formatting paths
            for (const auto& err : result.errors())
                static_cast<void>(err.message.size());
        }
    }

    return 0;
}
