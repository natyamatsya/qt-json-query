#pragma once

#include <QString>
#include <QJsonValue>
#include <QVector>
#include <optional>
#include <functional>

namespace json_query::json_path {
    // Forward declarations
    using FilterFn = std::function<bool(const QJsonValue&)>;
    struct Token;
}

namespace json_query::json_path::detail {

// Helper function for JSON number validation with strict JSON compliance
[[nodiscard]] bool isValidJsonNumber(const QString& value) noexcept;

// Helper function for unquoting strings
bool unquote(QString& s);

// A tiny façade so every parser can push a predicate and immediately obtain the corresponding Token
class Builder {
public:
    QVector<json_query::json_path::FilterFn>& fns;
    
    [[nodiscard]] json_query::json_path::Token add(json_query::json_path::FilterFn fn, QString key = {});
};

} // namespace json_query::json_path::detail
