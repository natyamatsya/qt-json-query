#pragma once
// JSONMatchersGTest.hpp
// ---------------------------------------------------------------------------
// Centralised helper utilities and lightweight GoogleTest/GMock matchers used
// across all test suites (conformance, Baeldung, Jayway parity, etc.).  The
// goal is to keep test code concise and dependency-free beyond GoogleTest, Qt
// Core and the standard library.
//
// Usage example:
//   #include "framework/JSONMatchersGTest.hpp"
//   auto doc  = parseJson(R"({"foo": "bar"})");
//   auto path = *JSONPath::create(u"$.foo");
//   QJsonValue v = eval(path, doc);
//   EXPECT_TRUE(IsString(v, u"bar"));
//
//   QJsonArray arr = evalArray(*JSONPath::create(u"$[*]"), doc);
//   EXPECT_TRUE(ContainsOnly(arr, 1, 2, 3));
// ---------------------------------------------------------------------------

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <type_traits>
#include <utility>
#include <initializer_list>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <vector>
#include <expected>
#include "json-query/json-path/JSONPath.hpp"

using json_query::JSONPath;
using json_query::json_path::Error;

// Parse raw JSON C-string into a QJsonDocument.
inline QJsonDocument parseJson(const char* src)
{
    QByteArray data(src);
    // Convenience: allow tests to use \" to embed quotes inside raw strings.
    data.replace("\\\"", "\"");
    return QJsonDocument::fromJson(data);
}

// ---------------------------------------------------------------------------
// Evaluation wrappers --------------------------------------------------------
// ---------------------------------------------------------------------------

// Evaluate a compiled JSONPath against a document and return the raw value.
inline QJsonValue eval(const JSONPath& path, const QJsonDocument& doc)
{
    return path.evaluate(doc);
}

// Convenience overload that compiles the path each call (avoid in hot code).
inline QJsonValue eval(QStringView path, const QJsonDocument& doc)
{
    auto p = JSONPath::create(path);
    if (!p) return {};
    return p->evaluate(doc);
}

// Return result as array – scalar values are wrapped into single-element array
// so caller can treat uniformly.
inline QJsonArray evalArray(const JSONPath& path, const QJsonDocument& doc)
{
    QJsonValue v = path.evaluate(doc);
    return v.isArray() ? v.toArray() : QJsonArray{v};
}

inline QJsonArray evalArray(QStringView path, const QJsonDocument& doc)
{
    auto p = JSONPath::create(path);
    if (!p) return {};
    return evalArray(*p, doc);
}

// ---------------------------------------------------------------------------
// Exception expectation ------------------------------------------------------
// ---------------------------------------------------------------------------
// Transition from throw-based to std::expected tests.  Provide helpers that
// check for specific json_query::Error codes instead of exceptions.

using EvalResult = std::expected<QJsonValue, Error>;

// Evaluate path and propagate compile-time errors via std::expected.  Runtime
// evaluation currently cannot fail (always returns a value), so we wrap it in
// a successful result for now.  When evaluation gains error handling this
// wrapper will forward those as well.
inline EvalResult evalExp(QStringView path,
                          const QJsonDocument& doc,
                          JSONPath::Option opt = JSONPath::Option::None)
{
    auto compiled = JSONPath::create(path, opt);
    if (!compiled)
        return std::unexpected(compiled.error());
    return compiled->evaluate(doc);
}

// Macro to assert that a statement of type std::expected<> fails with a
// specific json_query::Error code.
#define EXPECT_PATH_ERROR(expr, errEnum)                        \
    do {                                                        \
        auto _res = (expr);                                     \
        ASSERT_FALSE(_res.has_value())                          \
            << "Expected error but got success";                \
        EXPECT_EQ(_res.error(), (errEnum));                     \
    } while (false)

// ---------------------------------------------------------------------------
// Custom GoogleTest matchers ------------------------------------------------

// Matcher for JSON string equality
// Overload-friendly JSON string matcher: accepts QString, QStringView, or UTF-8 string literal
MATCHER_P(IsJsonString, expected, "JSON string equals")
{
    QString expectedStr;
    if constexpr (std::is_same_v<std::decay_t<decltype(expected)>, const char*>) {
        expectedStr = QString::fromUtf8(expected);
    } else {
        expectedStr = QString(expected);
    }
    return arg.isString() && arg.toString() == expectedStr;
}

// Convenience helper to build key/value pairs for JsonObjContains without std::initializer_list verbosity.
inline std::pair<QString, QJsonValue> kv(const char* key, const char* val)
{
    return { QString::fromUtf8(key), QJsonValue(QString::fromUtf8(val)) };
}

inline std::pair<QString, QJsonValue> kv(QStringView key, QStringView val)
{
    return { QString(key), QJsonValue(QString(val)) };
}

inline std::pair<QString, QJsonValue> kv(const char* key, int val)
{
    return { QString::fromUtf8(key), QJsonValue(val) };
}

inline std::pair<QString, QJsonValue> kv(QStringView key, int val)
{
    return { QString(key), QJsonValue(val) };
}

// Helper to assemble initializer_list without explicit type
template<typename... Pairs>
inline std::vector<std::pair<QString, QJsonValue>> kvlist(Pairs&&... pairs)
{
    return { std::forward<Pairs>(pairs)... };
}

MATCHER(IsJsonObject, "JSON object")
{
    return arg.isObject();
}

MATCHER_P(IsJsonInt, expected, "JSON int equals")
{
    return arg.isDouble() && arg.toInt() == expected;
}

MATCHER_P(JsonObjContains, kvPairs, "object contains key/value pairs")
{
    if (!arg.isObject()) return false;
    const auto obj = arg.toObject();
    for (const auto &pair : kvPairs)
    {
        const QString &key = pair.first;
        const QJsonValue &val = pair.second;
        if (!obj.contains(key) || obj.value(key) != val)
            return false;
    }
    return true;
}

// Matcher for undefined JSON value (Qt's isUndefined)
MATCHER(IsJsonUndefined, "JSON value is undefined")
{
    return arg.isUndefined();
}

// ---------------------------------------------------------------------------

// Verify QJsonValue is string and equals expected.
inline ::testing::AssertionResult IsString(const QJsonValue& v, QStringView expected)
{
    if (!v.isString())
        return ::testing::AssertionFailure() << "value is not a JSON string";
    if (v.toString() != expected)
        return ::testing::AssertionFailure() << "expected '" << expected.toString().toStdString()
                                            << "' but got '" << v.toString().toStdString() << "'";
    return ::testing::AssertionSuccess();
}

// Helper to compare that an array contains exactly the provided elements in
// any order (no duplicates unless provided).
inline bool containsAll(const QJsonArray& arr, const std::vector<QJsonValue>& expected)
{
    for (const auto& e : expected)
        if (!arr.contains(e)) return false;
    return true;
}

template <typename... T>
::testing::AssertionResult ContainsOnly(const QJsonArray& arr, const T&... elems)
{
    std::vector<QJsonValue> expected{ QJsonValue(elems)... };
    if (arr.size() != int(expected.size()))
        return ::testing::AssertionFailure() << "array size " << arr.size()
                                            << " differs from expected " << expected.size();
    if (!containsAll(arr, expected))
        return ::testing::AssertionFailure() << "array does not contain exactly the expected elements";
    return ::testing::AssertionSuccess();
}
