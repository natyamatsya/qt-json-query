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

namespace {
    // shared recursive descent implementation used by both overloads
    QJsonArray __evaluateRecursiveImpl(const QJsonValue& value)
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
            out.append(cur);

            if (cur.isObject()) {
                const QJsonObject obj = cur.toObject();
                for (auto it = obj.begin(); it != obj.end(); ++it) {
                    const QJsonValue& child = it.value();
                    if (child.isArray() || child.isObject())
                        queue.push_back(child);
                }
            } else {
                const QJsonArray arr = cur.toArray();
                for (const auto& child : arr)
                    if (child.isArray() || child.isObject())
                        queue.push_back(child);
            }
        }
        return out;
    }

    // internal helper to avoid code duplication
    QJsonArray __wildcardObjectImpl(const QJsonObject& obj)
    {
        QJsonArray out;
        for (auto it = obj.begin(); it != obj.end(); ++it)
            out.append(it.value());
        return out;
    }
}

QJsonArray wildcardObject(const QJsonObject& obj)
{
    return __wildcardObjectImpl(obj);
}

QJsonArray wildcardArray(const QJsonArray& arr)
{
    return arr; // shallow copy
}

QJsonArray evaluateRecursive(const json_query::JSONPath& /*jp*/, const QJsonValue& value, int /*unused*/)
{
    return __evaluateRecursiveImpl(value);
}

QJsonArray evaluateRecursive(const QJsonValue& value, int unused)
{
    Q_UNUSED(unused);
    return __evaluateRecursiveImpl(value);
}

} // namespace json_query::json_path::detail