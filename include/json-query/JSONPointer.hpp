#pragma once
#include <QJsonValue>
#include <QVector>
#include <optional>
#include <expected>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "json-query/json-pointer/JSONPointerParsing.hpp"
#include "json-query/json-pointer/JSONPointerEvaluation.hpp"

namespace json_query
{

class JSONPointer
{
public:
    using Error      = json_pointer::detail::ParseError; // alias internal
    using EvalError  = json_pointer::detail::EvalError;  // alias internal

    // Factory function mirroring JSONPath
    using Result     = std::expected<JSONPointer, Error>;
    using EvalResult = std::expected<QJsonValue, EvalError>;

    // Factory function mirroring JSONPath
    static Result create(QStringView pointer);

    // Detailed-error variants
    [[nodiscard]] EvalResult evaluate(QJsonDocument const&) const;
    [[nodiscard]] EvalResult evaluate(QJsonValue   const&) const;

    [[nodiscard]] QString toString() const;

private:
    JSONPointer() = default;  // internal default ctor for factory

    using Token = json_query::json_pointer::detail::Token;
    QVector<Token>   m_tokens;

    [[nodiscard]] std::expected<void, Error> parsePointer(QStringView);
    static void decodeAndStore(QStringView raw, QVector<Token>& out);
};

} // namespace json_query
