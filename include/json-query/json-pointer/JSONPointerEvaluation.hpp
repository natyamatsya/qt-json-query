#pragma once

#include <QJsonValue>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include "json-query/json-pointer/JSONPointerParsing.hpp"

namespace json_query::json_pointer::detail {

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

[[nodiscard]] inline QJsonValue evaluatePointer(const QVector<Token>& tokens, const QJsonValue& root) noexcept
{
    if (tokens.isEmpty()) return root;
    QJsonValue current{ root };
    for (const Token& tk : tokens)
    {
        switch (current.type())
        {
        case QJsonValue::Object:
            if (tk.kind != Token::Kind::Key || !stepObject(current, tk.key))
                return QJsonValue{QJsonValue::Undefined};
            break;
        case QJsonValue::Array:
            if (tk.kind != Token::Kind::Index || !stepArray(current, tk.index))
                return QJsonValue{QJsonValue::Undefined};
            break;
        default:
            return QJsonValue{QJsonValue::Undefined};
        }
    }
    return current;
}

} // namespace json_query::json_pointer::detail
