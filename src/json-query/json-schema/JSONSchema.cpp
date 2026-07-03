// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include "json-query/json-schema/JSONSchema.hpp"
#include "json-query/json-schema/JSONSchemaCompile.hpp"
#include "json-query/json-schema/JSONSchemaValidate.hpp"

#include "json-query/config/AbiNamespace.hpp"

namespace json_query::inline JSON_QUERY_ABI_NS::json_schema
{

JSONSchema::~JSONSchema() = default;

JSONSchema::JSONSchema(std::shared_ptr<const internal::CompiledSchema> compiled) : m_compiled(std::move(compiled)) {}

JSONSchema::ParseResult JSONSchema::create(const QJsonObject& schemaObject)
{
    return create(QJsonValue(schemaObject), {});
}

JSONSchema::ParseResult JSONSchema::create(const QJsonObject& schemaObject, SchemaFetcher fetcher)
{
    return create(QJsonValue(schemaObject), std::move(fetcher));
}

JSONSchema::ParseResult
JSONSchema::create(const QJsonObject& schemaObject, SchemaFetcher fetcher, SchemaOptions options)
{
    return create(QJsonValue(schemaObject), std::move(fetcher), options);
}

JSONSchema::ParseResult JSONSchema::create(const QJsonDocument& schemaDoc) { return create(schemaDoc, {}); }

JSONSchema::ParseResult JSONSchema::create(const QJsonDocument& schemaDoc, SchemaFetcher fetcher)
{
    return create(schemaDoc, std::move(fetcher), SchemaOptions{FormatValidation::Assertion});
}

JSONSchema::ParseResult
JSONSchema::create(const QJsonDocument& schemaDoc, SchemaFetcher fetcher, SchemaOptions options)
{
    if (schemaDoc.isObject())
        return create(schemaDoc.object(), std::move(fetcher), options);
    if (schemaDoc.isArray())
        return std::unexpected(Error(ParseError::InvalidSchemaStructure));
    return std::unexpected(Error(ParseError::EmptySchema));
}

JSONSchema::ParseResult JSONSchema::create(const QJsonValue& schemaValue) { return create(schemaValue, {}); }

JSONSchema::ParseResult JSONSchema::create(const QJsonValue& schemaValue, SchemaFetcher fetcher)
{
    return create(schemaValue, std::move(fetcher), SchemaOptions{FormatValidation::Assertion});
}

JSONSchema::ParseResult JSONSchema::create(const QJsonValue& schemaValue, SchemaFetcher fetcher, SchemaOptions options)
{
    auto compileResult{compileSchema(schemaValue, std::move(fetcher), options)};
    if (!compileResult)
        return std::unexpected(compileResult.error());

    return JSONSchema(std::move(*compileResult));
}

ValidationResult JSONSchema::validate(const QJsonValue& instance) const
{
    using json_query::literals::operator""_qt_s;

    if (!m_compiled)
    {
        ValidationResult result{};
        result.addError(u""_qt_s, u"#"_qt_s, u"Schema is not compiled"_qt_s, EvalError::ConstMismatch);
        return result;
    }

    return validateInstance(*m_compiled, instance);
}

ValidationResult JSONSchema::validate(const QJsonDocument& doc) const
{
    if (doc.isObject())
        return validate(QJsonValue(doc.object()));
    else if (doc.isArray())
        return validate(QJsonValue(doc.array()));
    else
        return validate(QJsonValue());
}

bool JSONSchema::isValid(const QJsonValue& instance) const
{
    if (!m_compiled)
        return false;

    return isInstanceValid(*m_compiled, instance);
}

QString JSONSchema::schemaId() const
{
    if (m_compiled)
        return m_compiled->schemaId;
    return {};
}

QString JSONSchema::schemaVersion() const
{
    if (m_compiled)
        return m_compiled->dialect;
    return {};
}

} // namespace json_query::json_schema
