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
    enum class Error : std::uint8_t {
        MissingLeadingSlash,
        EmptyNonTerminalToken,
        InvalidEscapeSequence,
        ArrayIndexOverflow
    };

    // Factory function mirroring JSONPath
    static std::expected<JSONPointer, Error> create(QStringView pointer);

    [[nodiscard]] QJsonValue evaluate(QJsonDocument const&) const;
    [[nodiscard]] QJsonValue evaluate(QJsonValue   const&) const;

    [[nodiscard]] QString toString() const;

private:
    JSONPointer() = default;  // internal default ctor for factory

    using Token = json_query::json_pointer::detail::Token;
    QVector<Token>   m_tokens;

    [[nodiscard]] std::expected<void, Error> parsePointer(QStringView);
    static Error              mapError(json_pointer::detail::ParseError);
    [[nodiscard]] QJsonValue  evaluateInternal(QJsonValue const&) const;
    static void decodeAndStore(QStringView raw, QVector<Token>& out);
};

} // namespace json_query
