// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception OR MIT

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <cstdio>
#include "json-query/JSONQuery"

using namespace json_query;

int main()
{
    const QJsonObject obj{{"items", QJsonArray{QJsonObject{{"id", 1}}, QJsonObject{{"id", 2}}}}};
    const QJsonDocument doc{obj};

    auto ptr{JSONPointer::create(u"/items/1/id")};
    auto path{JSONPath::create(u"$.items[*].id")};
    auto schema{json_schema::JSONSchema::create(QJsonObject{{"type", "object"}, {"required", QJsonArray{"items"}}})};
    if (!ptr || !path || !schema) { puts("FAIL create"); return 1; }

    auto pv{ptr->evaluate(doc)};
    auto pr{path->evaluate(doc)};
    bool ok = pv && pv->toInt() == 2 && pr && pr->size() == 2 && schema->isValid(QJsonValue{obj});
    puts(ok ? "CONSUMER OK: pointer=2, path matches=2, schema valid" : "FAIL results");
    return ok ? 0 : 1;
}
