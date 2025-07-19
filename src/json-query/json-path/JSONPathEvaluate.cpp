#include "json-query/json-path/JSONPathEvaluate.hpp"
#include "json-query/json-path/JSONPath.hpp"
#include <deque>

namespace json_query::json_path::detail {

QJsonArray evaluateAll(const json_query::JSONPath& jp, const QJsonDocument& document)
{
    return evaluateAll(jp, document.isArray() ? QJsonValue(document.array())
                                              : QJsonValue(document.object()));
}

QJsonArray evaluateAll(const json_query::JSONPath& jp, const QJsonValue& value)
{
    QJsonValue res = jp.evaluate(value);
    if (res.isArray())
        return res.toArray();
    if (res.isUndefined() || res.isNull())
        return {};
    return QJsonArray{res};
}

QJsonArray wildcardObject(const json_query::JSONPath& /*jp*/, const QJsonObject& obj)
{
    QJsonArray out;
    for (auto it = obj.begin(); it != obj.end(); ++it)
        out.append(it.value());
    return out;
}

QJsonArray wildcardArray(const json_query::JSONPath& /*jp*/, const QJsonArray& arr)
{
    return arr;                      // shallow copy; Qt is implicit‑shared
}

QJsonArray evaluateRecursive(const json_query::JSONPath& /*jp*/, const QJsonValue& value, int /*unused*/)
{
    QJsonArray out;
    if (!value.isArray() && !value.isObject())
        return out;

    std::deque<QJsonValue> queue;
    queue.push_back(value);
    while (!queue.empty())
    {
        QJsonValue cur = queue.front();
        queue.pop_front();
        // store the container itself
        out.append(cur);

        if (cur.isObject()) {
            const QJsonObject obj = cur.toObject();
            for (auto it = obj.begin(); it != obj.end(); ++it) {
                const QJsonValue& child = it.value();
                if (child.isArray() || child.isObject())
                    queue.push_back(child);
            }
        } else {                     // array
            const QJsonArray arr = cur.toArray();
            for (const auto& child : arr)
                if (child.isArray() || child.isObject())
                    queue.push_back(child);
        }
    }
    return out;
}

} // namespace json_query::json_path::detail