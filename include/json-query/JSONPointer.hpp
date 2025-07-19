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

    // Evaluation-time errors
    enum class EvalError : std::uint8_t {
        TypeMismatchObject,
        TypeMismatchArray,
        KeyNotFound,
        IndexOutOfRange
    };

    // Factory function mirroring JSONPath
    using Result = std::expected<JSONPointer, Error>;
    static Result create(QStringView pointer);

    // Alias for evaluation result with error
    using EvalResult = std::expected<QJsonValue, EvalError>;

    // Detailed-error variants
    [[nodiscard]] EvalResult evaluate(QJsonDocument const&) const;
    [[nodiscard]] EvalResult evaluate(QJsonValue   const&) const;

    [[nodiscard]] QString toString() const;

private:
    JSONPointer() = default;  // internal default ctor for factory

    using Token = json_query::json_pointer::detail::Token;
    QVector<Token>   m_tokens;

    [[nodiscard]] std::expected<void, Error> parsePointer(QStringView);
    static EvalError          mapEvalError(json_pointer::detail::EvalError);
    static Error              mapError(json_pointer::detail::ParseError);
    static void decodeAndStore(QStringView raw, QVector<Token>& out);
};

} // namespace json_query
