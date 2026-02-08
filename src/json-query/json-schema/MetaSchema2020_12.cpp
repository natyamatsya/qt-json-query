// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
// Built-in JSON Schema 2020-12 meta-schemas.
// Source: https://json-schema.org/draft/2020-12/schema
//
// Each schema is stored in a separate .json file under meta-schemas/
// and pulled in via #include as a raw string literal.

#include "json-query/json-schema/internal/MetaSchema2020_12.hpp"

#include <QJsonDocument>
#include <QJsonObject>
#include <array>
#include <cstring>
#include <utility>

namespace json_query::json_schema::internal
{

namespace
{

struct BuiltinSchema
{
    const char* uri;
    const char* json;
};

// clang-format off
static constexpr const char kSchema[] =
#include "meta-schemas/draft-2020-12/schema.inc"
;
static constexpr const char kMetaCore[] =
#include "meta-schemas/draft-2020-12/meta/core.inc"
;
static constexpr const char kMetaApplicator[] =
#include "meta-schemas/draft-2020-12/meta/applicator.inc"
;
static constexpr const char kMetaUnevaluated[] =
#include "meta-schemas/draft-2020-12/meta/unevaluated.inc"
;
static constexpr const char kMetaValidation[] =
#include "meta-schemas/draft-2020-12/meta/validation.inc"
;
static constexpr const char kMetaMetaData[] =
#include "meta-schemas/draft-2020-12/meta/meta-data.inc"
;
static constexpr const char kMetaFormatAnnotation[] =
#include "meta-schemas/draft-2020-12/meta/format-annotation.inc"
;
static constexpr const char kMetaContent[] =
#include "meta-schemas/draft-2020-12/meta/content.inc"
;

constexpr std::array kBuiltinSchemas{
    BuiltinSchema{"https://json-schema.org/draft/2020-12/schema",                kSchema},
    BuiltinSchema{"https://json-schema.org/draft/2020-12/meta/core",             kMetaCore},
    BuiltinSchema{"https://json-schema.org/draft/2020-12/meta/applicator",       kMetaApplicator},
    BuiltinSchema{"https://json-schema.org/draft/2020-12/meta/unevaluated",      kMetaUnevaluated},
    BuiltinSchema{"https://json-schema.org/draft/2020-12/meta/validation",       kMetaValidation},
    BuiltinSchema{"https://json-schema.org/draft/2020-12/meta/meta-data",        kMetaMetaData},
    BuiltinSchema{"https://json-schema.org/draft/2020-12/meta/format-annotation", kMetaFormatAnnotation},
    BuiltinSchema{"https://json-schema.org/draft/2020-12/meta/content",          kMetaContent},
};
// clang-format on

} // namespace

std::optional<QJsonValue> lookupBuiltinSchema(const QString& uri)
{
    for (const auto& [schemaUri, json] : kBuiltinSchemas)
    {
        if (uri == QString::fromUtf8(schemaUri))
        {
            const auto doc{QJsonDocument::fromJson(
                QByteArray::fromRawData(json, static_cast<qsizetype>(std::strlen(json))))};
            if (!doc.isNull())
                return QJsonValue{doc.object()};
        }
    }
    return std::nullopt;
}

} // namespace json_query::json_schema::internal
