#pragma once

#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include "json-query/json-pointer/JSONPointerParsing.hpp"
#include <expected>

namespace json_query::json_pointer::detail {

// Evaluation-time errors
enum class EvalError : std::uint8_t {
    TypeMismatchObject,
    TypeMismatchArray,
    KeyNotFound,
    IndexOutOfRange
};

[[nodiscard]] inline bool stepObject(QJsonValue& current, const QString& key) noexcept
{
    const QJsonObject obj{ current.toObject() };
    const auto it = obj.constFind(key);
    if (it == obj.constEnd()) return false;
    current = *it;
    return true;
}

[[nodiscard]] inline bool stepArray(QJsonValue& current, qsizetype index) noexcept
{
    const QJsonArray arr{ current.toArray() };
    if (index < 0 || index >= arr.size()) return false;
    current = arr.at(index);
    return true;
}

[[nodiscard]] inline std::expected<QJsonValue, EvalError>
evaluatePointer(const QVector<Token>& tokens, const QJsonValue& root) noexcept
{
    if (tokens.isEmpty()) return root; // success with root value
    QJsonValue current{ root };
    for (const Token& tk : tokens)
    {
        switch (current.type())
        {
        case QJsonValue::Object:
            if (tk.kind != Token::Kind::Key)
                return std::unexpected(EvalError::TypeMismatchObject);
            if (!stepObject(current, tk.key))
                return std::unexpected(EvalError::KeyNotFound);
            break;
        case QJsonValue::Array:
            if (tk.kind != Token::Kind::Index)
                return std::unexpected(EvalError::TypeMismatchArray);
            if (!stepArray(current, tk.index))
                return std::unexpected(EvalError::IndexOutOfRange);
            break;
        default:
            return std::unexpected(tk.kind==Token::Kind::Key ? EvalError::TypeMismatchObject : EvalError::TypeMismatchArray);
        }
    }
    return current; // success
}

} // namespace json_query::json_pointer::detail
