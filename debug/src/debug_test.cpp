// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "json-query/json-path/JSONPath.hpp"
#include <QJsonDocument>
#include <QDebug>
#include <iostream>

using namespace json_query;

QJsonDocument parseJson(const char* json) { return QJsonDocument::fromJson(QByteArray(json)); }

int main()
{
    auto path{JSONPath::create(u"$.foo.bar[5]")};
    if (!path)
    {
        std::cout << "Failed to create path\n";
        return 1;
    }

    std::cout << "Testing $.foo.bar[5] on different values:\n\n";

    // Test 1: null
    {
        auto res = path->evaluate(parseJson(R"({"foo":{"bar":null}})"));
        std::cout << "Test 1 - null: ";
        if (res)
            std::cout << "SUCCESS (unexpected!) - Value: " << res->toVariant().toString().toStdString() << "\n";
        else
            std::cout << "FAILED (expected) - Error: " << static_cast<int>(res.error()) << "\n";
    }

    // Test 2: number
    {
        auto res = path->evaluate(parseJson(R"({"foo":{"bar":4}})"));
        std::cout << "Test 2 - number: ";
        if (res)
            std::cout << "SUCCESS (unexpected!) - Value: " << res->toVariant().toString().toStdString() << "\n";
        else
            std::cout << "FAILED (expected) - Error: " << static_cast<int>(res.error()) << "\n";
    }

    // Test 3: empty array
    {
        auto res = path->evaluate(parseJson(R"({"foo":{"bar":[]}})"));
        std::cout << "Test 3 - empty array: ";
        if (res)
            std::cout << "SUCCESS (unexpected!) - Value: " << res->toVariant().toString().toStdString() << "\n";
        else
            std::cout << "FAILED (expected) - Error: " << static_cast<int>(res.error()) << "\n";
    }

    // Test 4: array with 5 elements
    {
        auto res = path->evaluate(parseJson(R"({"foo":{"bar":[1,2,3,4,5]}})"));
        std::cout << "Test 4 - array with 5 elements: ";
        if (res)
            std::cout << "SUCCESS (expected) - Value: " << res->toVariant().toString().toStdString() << "\n";
        else
            std::cout << "FAILED (unexpected!) - Error: " << static_cast<int>(res.error()) << "\n";
    }

    // Test 5: array with 6 elements
    {
        auto res = path->evaluate(parseJson(R"({"foo":{"bar":[1,2,3,4,5,6]}})"));
        std::cout << "Test 5 - array with 6 elements: ";
        if (res)
            std::cout << "SUCCESS (expected) - Value: " << res->toVariant().toString().toStdString() << "\n";
        else
            std::cout << "FAILED (unexpected!) - Error: " << static_cast<int>(res.error()) << "\n";
    }

    return 0;
}
