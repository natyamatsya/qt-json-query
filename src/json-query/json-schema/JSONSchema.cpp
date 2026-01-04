// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-schema/JSONSchema.hpp"
#include "json-query/json-schema/JSONSchemaCompile.hpp"
#include "json-query/json-schema/JSONSchemaValidate.hpp"

namespace json_query::json_schema
{

JSONSchema::ParseResult JSONSchema::create(const QJsonObject& schemaObject)
{
    return create(QJsonValue(schemaObject));
}

JSONSchema::ParseResult JSONSchema::create(const QJsonDocument& schemaDoc)
{
    if (schemaDoc.isObject())
    {
        return create(schemaDoc.object());
    }
    else if (schemaDoc.isArray())
    {
        // Arrays are not valid schemas
        return std::unexpected(QueryError(ParseError::InvalidSchemaStructure));
    }
    else
    {
        return std::unexpected(QueryError(ParseError::EmptySchema));
    }
}

JSONSchema::ParseResult JSONSchema::create(const QJsonValue& schemaValue)
{
    auto compileResult = compileSchema(schemaValue);
    if (!compileResult)
    {
        return std::unexpected(compileResult.error());
    }

    return JSONSchema(std::move(*compileResult));
}

ValidationResult JSONSchema::validate(const QJsonValue& instance) const
{
    if (!m_compiled)
    {
        ValidationResult result;
        result.addError(u""_qs, u"#"_qs, u"Schema is not compiled"_qs, EvalError::ConstMismatch);
        return result;
    }

    return validateInstance(*m_compiled, instance);
}

ValidationResult JSONSchema::validate(const QJsonDocument& doc) const
{
    if (doc.isObject())
    {
        return validate(QJsonValue(doc.object()));
    }
    else if (doc.isArray())
    {
        return validate(QJsonValue(doc.array()));
    }
    else
    {
        return validate(QJsonValue());
    }
}

bool JSONSchema::isValid(const QJsonValue& instance) const
{
    if (!m_compiled)
    {
        return false;
    }

    return isInstanceValid(*m_compiled, instance);
}

QString JSONSchema::schemaId() const
{
    if (m_compiled)
    {
        return m_compiled->schemaId;
    }
    return {};
}

QString JSONSchema::schemaVersion() const
{
    if (m_compiled)
    {
        return m_compiled->dialect;
    }
    return {};
}

} // namespace json_query::json_schema
