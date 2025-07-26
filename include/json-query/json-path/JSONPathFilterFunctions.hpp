#pragma once

#include <QString>
#include <QJsonValue>
#include <QVector>
#include <optional>
#include <functional>

namespace json_query::json_path {
    using FilterFn = std::function<bool(const QJsonValue&)>;
    struct Token;
}

namespace json_query::json_path::detail {

// Helper function to evaluate function calls like length(@.a) or value($..c)
// Refactored to use explicit error handling for JSONPath evaluation
QJsonValue evaluateFunction(const QString& funcExpr, const QJsonValue& context);

// Helper function to parse JSON literals from strings
// Refactored to use monadic error handling patterns
QJsonValue parseJsonLiteral(const QString& literal);

// Helper function to compare QJsonValues for ordering
int compareValues(const QJsonValue& left, const QJsonValue& right);

// Parse function calls like length() and value() in filter expressions
std::optional<json_query::json_path::Token> parseFunction(QString s, QVector<json_query::json_path::FilterFn>& out);

} // namespace json_query::json_path::detail
