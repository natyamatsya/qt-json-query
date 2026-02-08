// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#pragma once

#include "json-query/json-schema/internal/SchemaNode.hpp"
#include "json-query/utils/BraceSafe.hpp"
#include "json-query/utils/JSONError.hpp"

#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>
#include <QString>

#include <expected>
#include <optional>
#include <vector>

namespace json_query::json_schema::internal
{

/**
 * @brief Parse the 'type' keyword
 */
[[nodiscard]] inline std::expected<std::optional<TypeConstraint>, Error> parseTypeKeyword(const QJsonValue& typeValue)
{
    if (typeValue.isUndefined())
        return std::nullopt;

    TypeConstraint constraint{};

    if (typeValue.isString())
    {
        auto schemaType{stringToSchemaType(typeValue.toString())};
        if (!schemaType)
            return std::unexpected(Error(ParseError::InvalidTypeValue));
        constraint.allowedTypes.push_back(*schemaType);
        return constraint;
    }

    if (typeValue.isArray())
    {
        const auto typeArray{asArray(typeValue)};
        if (typeArray.isEmpty())
            return std::unexpected(Error(ParseError::InvalidTypeValue));

        for (const QJsonValue& typeItem : typeArray)
        {
            if (!typeItem.isString())
                return std::unexpected(Error(ParseError::InvalidTypeValue));
            auto schemaType{stringToSchemaType(typeItem.toString())};
            if (!schemaType)
                return std::unexpected(Error(ParseError::InvalidTypeValue));
            constraint.allowedTypes.push_back(*schemaType);
        }
        return constraint;
    }

    return std::unexpected(Error(ParseError::InvalidTypeValue));
}

/**
 * @brief Parse the 'enum' keyword
 */
[[nodiscard]] inline std::expected<std::optional<QJsonArray>, Error> parseEnumKeyword(const QJsonValue& enumValue)
{
    if (enumValue.isUndefined())
        return std::nullopt;

    if (!enumValue.isArray())
        return std::unexpected(Error(ParseError::InvalidEnumValue));

    const auto enumArray{asArray(enumValue)};
    if (enumArray.isEmpty())
        return std::unexpected(Error(ParseError::InvalidEnumValue));

    return enumArray;
}

/**
 * @brief Parse the 'const' keyword
 */
[[nodiscard]] inline std::optional<QJsonValue> parseConstKeyword(const QJsonValue& constValue)
{
    if (constValue.isUndefined())
        return std::nullopt;
    return constValue;
}

/**
 * @brief Parse numeric keywords (minimum, maximum, etc.)
 */
[[nodiscard]] inline std::expected<std::optional<double>, Error> parseNumericKeyword(const QJsonValue& value)
{
    if (value.isUndefined())
        return std::nullopt;

    if (!value.isDouble())
        return std::unexpected(Error(ParseError::InvalidKeywordValue));

    return value.toDouble();
}

/**
 * @brief Parse integer keywords (minLength, maxLength, minItems, etc.)
 */
[[nodiscard]] inline std::expected<std::optional<std::size_t>, Error> parseIntegerKeyword(const QJsonValue& value)
{
    if (value.isUndefined())
        return std::nullopt;

    if (!value.isDouble())
        return std::unexpected(Error(ParseError::InvalidKeywordValue));

    const auto d{value.toDouble()};
    if (d < 0 || d != static_cast<double>(static_cast<std::size_t>(d)))
        return std::unexpected(Error(ParseError::InvalidKeywordValue));

    return static_cast<std::size_t>(d);
}

/**
 * @brief Parse the 'pattern' keyword
 */
[[nodiscard]] inline std::expected<std::optional<SchemaRegex>, Error>
parsePatternKeyword(const QJsonValue& patternValue)
{
    if (patternValue.isUndefined())
        return std::nullopt;

    if (!patternValue.isString())
        return std::unexpected(Error(ParseError::InvalidKeywordValue));

    SchemaRegex regex{};
    if (!regex.compile(patternValue.toString()))
        return std::unexpected(Error(ParseError::InvalidRegexPattern));

    return regex;
}

/**
 * @brief Parse the 'required' keyword
 */
[[nodiscard]] inline std::expected<std::vector<QString>, Error> parseRequiredKeyword(const QJsonValue& requiredValue)
{
    std::vector<QString> result{};

    if (requiredValue.isUndefined())
        return result;

    if (!requiredValue.isArray())
        return std::unexpected(Error(ParseError::InvalidKeywordValue));

    const auto requiredArray{asArray(requiredValue)};
    result.reserve(static_cast<std::size_t>(requiredArray.size()));

    for (const QJsonValue& item : requiredArray)
    {
        if (!item.isString())
            return std::unexpected(Error(ParseError::InvalidKeywordValue));
        result.push_back(item.toString());
    }

    return result;
}

} // namespace json_query::json_schema::internal
